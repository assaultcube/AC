#include "cube.h"
#include "cmodserver.h"

string ut[] = { "null", "client", "server" };

string mt[] = {"register", "getlist"};

string rt[] = {"speedhack", "aimbot", "weaponhack", "wallhack", "maphack", "opk", "teleport", "serverhack", "bug_exploit", "other"};

string rm[] = {"Success", "Failed", "Ignoring", "Expired", "Something weird happened", "For sure!", "Yes", "No", "This guy is blacklisted!", "Whitelisted"};

inline uint8_t umin8 (uint8_t a, uint8_t b){
    if (a < b) return a;
    return b;
}

/* the exact type would be int33_t */
int64_t compare_ips (uint8_t *a, uint8_t *b) // little endian ips
{
    uint32_t ip1 = *(uint32_t *)a;
    uint32_t ip2 = *(uint32_t *)b;
    int r = 32 - umin8(a[4],b[4]);
    return ((int64_t)(ip1 >> r) - (int64_t)(ip2 >> r));
}

void init_msg_structs ( char *message, s_vmsg *vmsg ) {
    vmsg->mt = (uint16_t *) &message[0];
    vmsg->le = (uint8_t *) &message[0];
    vmsg->be = (uint8_t *) &message[1];
    vmsg->n = (uint32_t *) &message[2];
    vmsg->port = (uint16_t *) &message[2];
    vmsg->v = (uint8_t *) &message[2];
    /* 3 field messages: get_bl, remove_bl/wl */
    vmsg->t = (uint8_t *) &message[6];
    vmsg->k = (uint16_t *) &message[6];
    /* can trust */
    vmsg->ct = (uint8_t *) &message[6];
    /* 5/6 field messages: register, insert_wl */
    vmsg->name = (char *) &message[7];
    vmsg->version = (uint16_t *) &message[7+NAMELEN];
    /* 7 field messages: insert bl */
    vmsg->reason = (uint8_t *) &message[7+NAMELEN];
    vmsg->f = (uint8_t *) &message[8+NAMELEN]; // this is a vector of 5 elements
    vmsg->exp = (uint8_t *) &message[13+NAMELEN];
}

int msg_size (int v)
{
    switch (v) {
        case MT_REGISTER: // mt n t name v
            return 10+NAMELEN;
            break;
        case MT_GETLIST: // mt n t
            return 7;
            break;
        default:
            return MAXMSGSIZE;
    }
}

void invert_end32 (uint8_t *v)
{
    v[0] = v[0] ^ v[3];
    v[3] = v[0] ^ v[3];
    v[0] = v[0] ^ v[3];
    v[1] = v[1] ^ v[2];
    v[2] = v[1] ^ v[2];
    v[1] = v[1] ^ v[2];
}

void invert_end16 (uint8_t *v)
{
    v[0] = v[0] ^ v[1];
    v[1] = v[0] ^ v[1];
    v[0] = v[0] ^ v[1];
}

void fix_end (int mt, s_vmsg *vmsg)
{
    switch ( mt ) {
        case MT_REGISTER:
            invert_end16((uint8_t *)vmsg->version);
            break;
        default:
            break;
    }
}

int convert_ip ( char *string, uint8_t *f ) // little endian
{
    f[0] = f[1] = f[2] = f[3] = 0;
    f[4] = 32;

    if ( strchr(string,'/') == NULL ) {
        if ( sscanf (string,"%hhu.%hhu.%hhu.%hhu",&f[3],&f[2],&f[1],&f[0]) < 4 ) return 1;
    } else {
        if ( sscanf (string,"%hhu.%hhu.%hhu.%hhu/%hhu",&f[3],&f[2],&f[1],&f[0],&f[4]) < 5 || f[4] > 32 ) return 1;
    }
    return 0;
}

int find_reason (char *reason)
{
    int i;
    for ( i = 0; i < RT_LAST; i++ ) {
        char *rs = reason;
        const char *r = rt[i];
        while ( *rs != '\0' || *r != '\0' ) {
            if ( *rs != *r ) break;
            rs++;             // VEJA AQUI
            r++;
            if ( *rs == '\0' && *rs == '\0' ) return i;
        }
    }
    return -1;
}

int include_endian(s_vmsg *vmsg){
    if ( *vmsg->mt == *vmsg->le ) {
        *vmsg->be = 128;
        return 0;
    } else if ( *vmsg->mt == *vmsg->be ) {
        *vmsg->le = 128;
        return 1;
    } else {
        return -1;
    }
}

uint id_number = 12345; // this is a test
ENetAddress cmodaddress = { ENET_HOST_ANY, AC_CMOD_PORT }; //CMOD

int communicate(ENetAddress *addr, const char *host, ENetBuffer *buf) {

    if (addr->host == ENET_HOST_ANY && enet_address_set_host(addr, host)<0 ) {
        addr->host = ENET_HOST_ANY;
        printf(" * Error: Host %s cannot be resolved!\n",host);
        return RM_FAIL;
    }

    int sockfd = enet_socket_create(ENET_SOCKET_TYPE_STREAM);

    if (sockfd == ENET_SOCKET_NULL) {
        printf(" * Error in creating socket.\n");
        enet_socket_destroy(sockfd);
        return RM_FAIL;
    }

    int n, m = 0;
    while ( (n = enet_socket_connect(sockfd, addr)) < 0 && m < 10 ) m++;
    if ( n < 0 ) {
        printf(" * Error in connecting.\n");
        enet_socket_destroy(sockfd);
        return RM_FAIL;
    }

    n = enet_socket_send(sockfd, addr, buf, 1);
    if (n < 0) {
        printf(" * Error on write.\n");
        enet_socket_destroy(sockfd);
        return RM_FAIL;
    }

    bzero(buf->data, 1);
    buf->dataLength = 1;

    n = enet_socket_receive(sockfd, addr, buf, 1);

    if ( n == 0 ) {
        printf(" * Error: No response from %s\n",host);
        enet_socket_destroy(sockfd);
        return RM_FAIL;
    } else if ( n < 0 ) {
        printf(" * Error in reading from socket\n");
        enet_socket_destroy(sockfd);
        return RM_FAIL;
    }

    union {
        int8_t d;
        char c[2];
    } reply;

    reply.d = RM_FAIL;

    strncpy(reply.c,(char *)buf->data,1);

    if ( reply.d >= 0 && reply.d < RM_LAST ) {
        printf(" ** Communication %s\n",rm[reply.d]);
        enet_socket_destroy(sockfd);
        return reply.d;
    }
    else printf(" * Error: Server sent an unexpected reply. Report this incident.\n");

    enet_socket_destroy(sockfd);
    return RM_FAIL;
}

extern char *global_name;

int cmodregister(int type)
{
    ENetBuffer buf;
    s_vmsg vmsg;
    char message[MAXMSGSIZE];
    init_msg_structs (message, &vmsg);
    buf.data = message;

    bzero(message,msg_size(MT_REGISTER));
    buf.dataLength = msg_size(MT_REGISTER);

    *vmsg.mt = MT_REGISTER;
    *vmsg.n = id_number;

    if ( include_endian(&vmsg) < 0 ) {
        printf(" * Weird endianess. Please, report this incident.\n");
        return RM_FAIL;
    }

    strncpy(vmsg.name,global_name,NAMELEN-1);
    *vmsg.t = type;
    *vmsg.version = AC_VERSION;

    return communicate(&cmodaddress, AC_CMOD_URI, &buf);
}

int lastcmodreg = 0;
int cmodregistered = RM_DONE;

int updatecmod(int millis, int type)
{
    if ( !lastcmodreg || lastcmodreg + 1000 * 60 * 20 + 3333 < millis ) { // about 20 minutes (with some error)
	cmodregistered = cmodregister(type);
	if ( cmodregistered != RM_DONE ) {
	    //treat_failure(can_retrieve);
	    printf("You cannot register in the masterserver\n");
	    return 0;
	} else lastcmodreg = millis;
    }
    return 1;
}
