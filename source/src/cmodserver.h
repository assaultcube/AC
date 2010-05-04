/* http://www.linuxhowtos.org/C_C++/socket.htm */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>  // sleep
#include <ctype.h>

#define MAXCOL 300
#define IPLEN 21
#define NAMELEN 21
#define MAXBUF 1024
#define MAXBLTIME 30 // days
#define MAXMSGSIZE 25+NAMELEN

/** this is a test*/
#define AC_CMOD_URI "ac.if.usp.br"
#define AC_CMOD_PORT 28763
#define AC_MS_URI "ac.if.usp.br"
#define AC_MS_PORT 28764


enum
{UT_NULL, UT_CLIENT, UT_SERVER, UT_LAST};

enum
{MT_REGISTER=1, MT_GETLIST, MT_LAST};

enum
{RT_SPEED, RT_AIMBOT, RT_WEAPON, RT_WALL, RT_MAP, RT_OPK, RT_TELEPORT, RT_SERVER, RT_BUGEXP, RT_OTHER, RT_LAST};

enum
{RM_DONE, RM_FAIL, RM_IGN, RM_EXPIRED, RM_TRAGEDY, RM_FORSURE, RM_YES, RM_NO, RM_BLED, RM_WLED, RM_LAST};


typedef struct {
    uint16_t *mt;                        // the message type
    uint8_t *le;                         // endianess
    uint8_t *be;                         // endianess
    uint32_t *n;                         // user id (client side)
    uint16_t *port;
    uint8_t *v;                          // endian verifier
    uint8_t *t;                          // ut for get_bl
    uint16_t *k;                         // index for remove_bl/wl
    uint8_t *ct;                         // can trust ip 
    char *name;
    uint16_t *version;
    uint8_t *f;                          // ip fields
    uint8_t *reason;
    uint8_t *exp;                       // expiration in days
} s_vmsg;


