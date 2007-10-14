#define MAXCLIENTS 256                  // in a multiplayer game, can be arbitrarily changed
#define DEFAULTCLIENTS 6
#define MAXTRANS 5000                   // max amount of data to swallow in 1 go
#define CUBE_DEFAULT_SERVER_PORT 28763
#define CUBE_SERVINFO_PORT(serverport) (serverport+1)
#define PROTOCOL_VERSION 1126           // bump when protocol changes
#define DEMO_VERSION 1                  // bump when demo format changes
#define DEMO_MAGIC "ASSAULTCUBE_DEMO"

// network messages codes, c2s, c2c, s2c
enum
{
    SV_INITS2C = 0, SV_INITC2S, SV_POS, SV_TEXT, SV_TEAMTEXT, SV_SOUND, SV_CDIS,
    SV_SHOOT, SV_EXPLODE, SV_SUICIDE, SV_AKIMBO, SV_RELOAD,
    SV_GIBDIED, SV_DIED, SV_GIBDAMAGE, SV_DAMAGE, SV_HITPUSH, SV_SHOTFX, SV_THROWNADE,
    SV_TRYSPAWN, SV_SPAWNSTATE, SV_SPAWN, SV_FORCEDEATH, SV_RESUME,
    SV_TIMEUP, SV_EDITENT, SV_MAPRELOAD, SV_ITEMACC,
    SV_MAPCHANGE, SV_ITEMSPAWN, SV_ITEMPICKUP,
    SV_PING, SV_PONG, SV_CLIENTPING, SV_GAMEMODE,
    SV_EDITMODE, SV_EDITH, SV_EDITT, SV_EDITS, SV_EDITD, SV_EDITE,
    SV_SENDMAP, SV_RECVMAP, SV_SERVMSG, SV_ITEMLIST, SV_WEAPCHANGE, SV_PRIMARYWEAP,
    SV_MODELSKIN,
    SV_FLAGPICKUP, SV_FLAGDROP, SV_FLAGRETURN, SV_FLAGSCORE, SV_FLAGRESET, SV_FLAGINFO, SV_FLAGS,
    SV_ARENAWIN,
	SV_SETMASTER, SV_SETADMIN, SV_SERVOPINFO, 
    SV_CALLVOTE, SV_CALLVOTESUC, SV_CALLVOTEERR, SV_VOTE, SV_VOTERESULT,
	SV_FORCETEAM, SV_AUTOTEAM,
    SV_WHOIS, SV_WHOISINFO,
    SV_LISTDEMOS, SV_SENDDEMOLIST, SV_GETDEMO, SV_SENDDEMO, SV_DEMOPLAYBACK,
    SV_CONNECT,
    SV_CLIENT
};

enum { SA_KICK = 0, SA_BAN, SA_REMBANS, SA_MASTERMODE, SA_AUTOTEAM, SA_FORCETEAM, SA_GIVEMASTER, SA_MAP, SA_RECORDDEMO, SA_STOPDEMO, SA_CLEARDEMOS, SA_NUM};
enum { VOTE_NEUTRAL = 0, VOTE_YES, VOTE_NO, VOTE_NUM };
enum { VOTEE_DISABLED = 0, VOTEE_CUR, VOTEE_MUL, VOTEE_MAX, VOTEE_DED, VOTEE_NUM };
enum { MM_OPEN, MM_PRIVATE, MM_NUM };

#define DMF 16.0f 
#define DNF 100.0f
#define DVELF 4.0f

enum { DISC_NONE = 0, DISC_EOP, DISC_CN, DISC_MKICK, DISC_MBAN, DISC_TAGT, DISC_BANREFUSE, DISC_WRONGPW, DISC_SOPLOGINFAIL, DISC_MAXCLIENTS, DISC_MASTERMODE, DISC_AUTOKICK, DISC_NUM };

/* Gamemodes
0   tdm
1   coop edit
2   dm
3   survivor
4   team survior
5   ctf
6   pistols
7   bot tdm
8   bot dm
9   last swiss standing
10  one shot, one kill
11  team one shot, one kill
12  bot one shot, one kill
*/

#define m_lms         (gamemode==3 || gamemode==4)
#define m_ctf         (gamemode==5)
#define m_pistol      (gamemode==6)
#define m_lss         (gamemode==9)
#define m_osok        (gamemode>=10 && gamemode<=12)

#define m_noitems     (m_lms || m_osok)
#define m_noitemsnade (m_lss)
#define m_nopistol    (m_osok || m_lss)
#define m_noprimary   (m_pistol || m_lss)
#define m_noguns      (m_nopistol && m_noprimary)
#define m_arena       (m_lms || m_lss || m_osok)
#define m_teammode    (gamemode==0 || gamemode==4 || gamemode==5 || gamemode==7 || gamemode==11)
#define m_tarena      (m_arena && m_teammode)
#define m_botmode     (gamemode==7 || gamemode == 8 || gamemode==12)
#define m_valid(mode) ((mode)>=0 && (mode)<=12 || (mode) == -3)
#define m_mp(mode)    (m_valid(mode) && (mode)!=7 && (mode)!=8 && (mode)!=12)
#define m_demo        (gamemode==-3)
