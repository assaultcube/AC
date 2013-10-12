#define MAXCLIENTS 256                  // in a multiplayer game, can be arbitrarily changed
#define DEFAULTCLIENTS 6
#define MAXTRANS 5000                   // max amount of data to swallow in 1 go
#define CUBE_DEFAULT_SERVER_PORT 28763
#define CUBE_SERVINFO_PORT_LAN 28762
#define CUBE_SERVINFO_PORT(serverport) (serverport+1)
#define CUBE_SERVINFO_TO_SERV_PORT(servinfoport) (servinfoport-1)
#define PROTOCOL_VERSION 1201           // bump when protocol changes (use negative numbers for mods!)
#define DEMO_VERSION 2                  // bump when demo format changes
#define DEMO_MAGIC "ASSAULTCUBE_DEMO"
#define DEMO_MINTIME 10000              // don't keep demo recordings with less than 10 seconds
#define MAXMAPSENDSIZE 65536
#define MAXCFGFILESIZE 65536

extern bool modprotocol;
#define CUR_PROTOCOL_VERSION (modprotocol ? -PROTOCOL_VERSION : PROTOCOL_VERSION)

// network messages codes, c2s, c2c, s2c
enum
{
    SV_SERVINFO = 0, SV_WELCOME, SV_INITCLIENT, SV_POS, SV_POSC, SV_POSN, SV_TEXT, SV_TEAMTEXT, SV_TEXTME, SV_TEAMTEXTME, SV_TEXTPRIVATE,
    SV_SOUND, SV_VOICECOM, SV_VOICECOMTEAM, SV_CDIS,
    SV_SHOOT, SV_EXPLODE, SV_SUICIDE, SV_AKIMBO, SV_RELOAD, SV_AUTHT, SV_AUTHREQ, SV_AUTHTRY, SV_AUTHANS, SV_AUTHCHAL,
    SV_GIBDIED, SV_DIED, SV_GIBDAMAGE, SV_DAMAGE, SV_HITPUSH, SV_SHOTFX, SV_THROWNADE,
    SV_TRYSPAWN, SV_SPAWNSTATE, SV_SPAWN, SV_SPAWNDENY, SV_FORCEDEATH, SV_RESUME,
    SV_DISCSCORES, SV_TIMEUP, SV_EDITENT, SV_ITEMACC,
    SV_MAPCHANGE, SV_ITEMSPAWN, SV_ITEMPICKUP,
    SV_PING, SV_PONG, SV_CLIENTPING, SV_GAMEMODE,
    SV_EDITMODE, SV_EDITH, SV_EDITT, SV_EDITS, SV_EDITD, SV_EDITE, SV_NEWMAP,
    SV_SENDMAP, SV_RECVMAP, SV_REMOVEMAP,
    SV_SERVMSG, SV_ITEMLIST, SV_WEAPCHANGE, SV_PRIMARYWEAP,
    SV_FLAGACTION, SV_FLAGINFO, SV_FLAGMSG, SV_FLAGCNT,
    SV_ARENAWIN,
    SV_SETADMIN, SV_SERVOPINFO,
    SV_CALLVOTE, SV_CALLVOTESUC, SV_CALLVOTEERR, SV_VOTE, SV_VOTERESULT,
    SV_SETTEAM, SV_TEAMDENY, SV_SERVERMODE,
    SV_IPLIST,
    SV_LISTDEMOS, SV_SENDDEMOLIST, SV_GETDEMO, SV_SENDDEMO, SV_DEMOPLAYBACK,
    SV_CONNECT,
    SV_SWITCHNAME, SV_SWITCHSKIN, SV_SWITCHTEAM,
    SV_CLIENT,
    SV_EXTENSION,
    SV_MAPIDENT, SV_HUDEXTRAS, SV_POINTS,
    SV_NUM
};

#ifdef _DEBUG

extern void protocoldebug(bool enable);

// converts message code to char
extern const char *messagenames[SV_NUM];
#endif

enum { SA_KICK = 0, SA_BAN, SA_REMBANS, SA_MASTERMODE, SA_AUTOTEAM, SA_FORCETEAM, SA_GIVEADMIN, SA_MAP, SA_RECORDDEMO, SA_STOPDEMO, SA_CLEARDEMOS, SA_SERVERDESC, SA_SHUFFLETEAMS, SA_NUM};
enum { VOTE_NEUTRAL = 0, VOTE_YES, VOTE_NO, VOTE_NUM };
enum { VOTEE_DISABLED = 0, VOTEE_CUR, VOTEE_MUL, VOTEE_MAX, VOTEE_AREA, VOTEE_PERMISSION, VOTEE_INVALID, VOTEE_WEAK, VOTEE_NEXT, VOTEE_NUM };
enum { MM_OPEN = 0, MM_PRIVATE, MM_MATCH, MM_NUM }; enum { MM_MASK = 0x03 };
enum { AT_DISABLED = 0, AT_ENABLED = 1, AT_SHUFFLE = 2, AT_NUM };
enum { FA_PICKUP = 0, FA_STEAL, FA_DROP, FA_LOST, FA_RETURN, FA_SCORE, FA_NUM, FA_RESET };
enum { FM_PICKUP = 0, FM_DROP, FM_LOST, FM_RETURN, FM_SCORE, FM_KTFSCORE, FM_SCOREFAIL, FM_RESET, FM_NUM };
enum { FTR_INFO = 0, FTR_PLAYERWISH, FTR_AUTOTEAM, FTR_SILENTFORCE, FTR_NUM }; // forceteam-reasons
enum { SP_OK = 0, SP_REFILLMATCH, SP_OK_NUM, SP_WRONGMAP, SP_SPECT, SP_NUM }; // spawn permission (<OK_NUM && isspect: change to active team allowed)
enum { HE_COMBO, HE_COMBO2, HE_COMBO3, HE_COMBO4, HE_COMBO5, HE_TEAMWORK, HE_FLAGDEFENDED, HE_FLAGCOVERED, HE_COVER, HE_NUM };


#define DMF 16.0f
#define DNF 100.0f
#define DVELF 4.0f

enum { DISC_NONE = 0, DISC_EOP, DISC_CN, DISC_MKICK, DISC_MBAN, DISC_TAGT, DISC_BANREFUSE, DISC_WRONGPW, DISC_SOPLOGINFAIL, DISC_MAXCLIENTS, DISC_MASTERMODE, DISC_AUTOKICK, DISC_AUTOBAN, DISC_DUP, DISC_BADNICK, DISC_OVERFLOW, DISC_ABUSE, DISC_AFK, DISC_FFIRE, DISC_CHEAT, DISC_NUM };
enum { BAN_NONE = 0, BAN_VOTE, BAN_AUTO, BAN_BLACKLIST, BAN_MASTER };

#define EXT_ACK                         -1
#define EXT_VERSION                     104
#define EXT_ERROR_NONE                  0
#define EXT_ERROR                       1
#define EXT_PLAYERSTATS_RESP_IDS        -10
#define EXT_UPTIME                      0
#define EXT_PLAYERSTATS                 1
#define EXT_TEAMSCORE                   2
#define EXT_PLAYERSTATS_RESP_STATS      -11

enum { PONGFLAG_PASSWORD = 0, PONGFLAG_BANNED, PONGFLAG_BLACKLIST, PONGFLAG_MASTERMODE = 6, PONGFLAG_NUM };
enum { EXTPING_NOP = 0, EXTPING_NAMELIST, EXTPING_SERVERINFO, EXTPING_MAPROT, EXTPING_UPLINKSTATS, EXTPING_NUM };

enum
{
    GMODE_DEMO = -1,
    GMODE_TEAMDEATHMATCH = 0,           // 0
    GMODE_COOPEDIT,
    GMODE_DEATHMATCH,
    GMODE_SURVIVOR,
    GMODE_TEAMSURVIVOR,
    GMODE_CTF,                          // 5
    GMODE_PISTOLFRENZY,
    GMODE_BOTTEAMDEATHMATCH,
    GMODE_BOTDEATHMATCH,
    GMODE_LASTSWISSSTANDING,
    GMODE_ONESHOTONEKILL,               // 10
    GMODE_TEAMONESHOTONEKILL,
    GMODE_BOTONESHOTONEKILL,
    GMODE_HUNTTHEFLAG,
    GMODE_TEAMKEEPTHEFLAG,
    GMODE_KEEPTHEFLAG,                  // 15
    GMODE_TEAMPF,
    GMODE_TEAMLSS,
    GMODE_BOTPISTOLFRENZY,
    GMODE_BOTLSS,
    GMODE_BOTTEAMSURVIVOR,              // 20
    GMODE_BOTTEAMONESHOTONKILL,
    GMODE_NUM
};

#define m_lms         (gamemode==3 || gamemode==4 || gamemode==20)
#define m_ctf         (gamemode==5)
#define m_pistol      (gamemode==6 || gamemode==16 || gamemode==18)
#define m_lss         (gamemode==9 || gamemode==17 || gamemode==19)
#define m_osok        ((gamemode>=10 && gamemode<=12) || gamemode==21)
#define m_htf         (gamemode==13)
#define m_ktf         (gamemode==14 || gamemode==15)

#define m_noitems     (m_lms || m_osok)
#define m_noitemsnade (m_lss)
#define m_nopistol    (m_osok || m_lss)
#define m_noprimary   (m_pistol || m_lss)
#define m_noguns      (m_nopistol && m_noprimary)
#define m_arena       (m_lms || m_lss || m_osok)
#define m_teammode    (gamemode==0 || gamemode==4 || gamemode==5 || gamemode==7 || gamemode==11 || gamemode==13 || gamemode==14 || gamemode==16 || gamemode==17 || gamemode==20 || gamemode==21)
#define m_tarena      (m_arena && m_teammode)
#define m_botmode     (gamemode==7 || gamemode == 8 || gamemode==12 || (gamemode>=18 && gamemode<=21))
#define m_valid(mode) (((mode)>=0 && (mode)<GMODE_NUM) || (mode) == -1)
#define m_mp(mode)    (m_valid(mode) && (mode)>=0 && (mode)!=7 && (mode)!=8 && (mode)!=12 && ((mode)<18 || (mode)>21))
#define m_demo        (gamemode==-1)
#define m_coop        (gamemode==1)
#define m_flags       (m_ctf || m_htf || m_ktf)

