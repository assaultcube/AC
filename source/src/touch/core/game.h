// handles interactions from the touch UI towards the game
struct game
{
    // load and save settings from the touch UI
    struct settings
    {
        int weapon = -1;

        enum skintype : int { FIRST = 0, SECOND, THIRD };
        skintype skin = FIRST;

        enum teamtype : int { CLA = 0, RVSF };
        teamtype team = CLA;

        int pointerspeed = 0; // values 0-100, default 30
        int pointeracceleration = 0; // values 0-100, default 0
        bool volumeup = false;
        bool devmode = false;

        void load()
        {
            weapon = player1->nextprimary;
            team = player1->team == TEAM_CLA || player1->team == TEAM_CLA_SPECT ? CLA : RVSF;
            int skinid = player1->skin(player1->team);
            if(team == CLA) skin = (skinid == 0 ? FIRST  : (skin == 1 ? SECOND : THIRD));
            else skin = (skinid == 0 ? FIRST  : (skin == 5 ? SECOND : THIRD));

            pointerspeed = clamp(int((sensitivity + 1.0f) * 10.0f), 0, 100);
            pointeracceleration = mouseaccel;

            extern int volumeupattack;
            this->volumeup = volumeupattack;
            this->devmode = !hideconsole;
        }

        void save()
        {
            newteam((char*)(team == CLA ?  "CLA" : "RVSF"));

            int skinid = -1;
            if(team == CLA) skinid = skin == FIRST ? 0 : skin == SECOND ? 1 : 5;
            else skinid = skin == FIRST ? 0 : skin == SECOND ? 5 : 7;

            player1->setskin(player1->team, skinid);

            // todo: merge this with game code
            setsvar("nextprimary", guns[weapon].modelname);
            player1->setnextprimary(weapon);
            addmsg(SV_PRIMARYWEAP, "ri", player1->nextprimweap->type);
            extern char *nextprimary;
            nextprimary = exchangestr(nextprimary, gunnames[player1->nextprimweap->type]);

            extern float sensitivity, mouseaccel;
            sensitivity = int(pointerspeed / 10.0f) - 1.0f;
            mouseaccel = pointeracceleration;

            extern int volumeupattack;
            volumeupattack = this->volumeup ? 1 : 0;
            hideconsole = (this->devmode ? 0 : 1);

            writecfg();
        }
    } settings;

    // launch a new game from the touch UI
    struct newgame
    {
        enum difficulty { EASY = 0, NORMAL, HARD };

        // launch a training with no enemies
        void training(const char *name, int team, int weapon, const char *map)
        {
            extern void kickallbots();
            extern void mode(int *n);

            // clean the scene
            trydisconnect();
            kickallbots();

            // Reliably set primary weapon and team as a single synchronous transaction to prevent undesired side effects.
            // undesired side effects are things like: a) death due to team change and b) outdated primary weapon c) data loss
            // because callvote(..) will drop queued messages d) visual flickering during mapload.
            // Generally it would be better to have a variaton of the callvote(..) method that does not drop queued messages so
            // that we can use the ordinary methods to change team and primary weapon.
            extern char *nextprimary;
            nextprimary = exchangestr(nextprimary, gunnames[weapon]);
            player1->setnextprimary(weapon);
            packetbuf p(MAXTRANS, ENET_PACKET_FLAG_RELIABLE);
            putint(p, SV_PRIMARYWEAP);
            putint(p, weapon);
            putint(p, SV_SWITCHTEAM);
            putint(p, team);
            sendpackettoserv(1, p.finalize());

            // change map
            string nextmode;
            itoa(nextmode, config.PLAYOFFLINE_GAMEMODE);
            string nexttimelimit;
            itoa(nexttimelimit, config.PLAYOFFLINE_TIMELIMIT);
            callvote(SA_MAP, map, nextmode, nexttimelimit);
        }

        // play offline against bots
        void playoffline(const char *name, int team, int weapon, const char *map, difficulty difficulty)
        {
            training(name, team, weapon, map);

            // map difficulty level to bot skill
            const char *botskill = config.PLAYOFFLINE_BOTSKILLS[difficulty];

            // add bots to team, subtract 1 from own team to account for the current player
            defformatstring(rvsfteamsize)("%i", config.PLAYOFFLINE_TEAMSIZE - (team == TEAM_RVSF ? 1 : 0));
            defformatstring(clateamsize)("%i", config.PLAYOFFLINE_TEAMSIZE - (team == TEAM_RVSF ? 0 : 1));
            addnbot(rvsfteamsize, teamnames[TEAM_RVSF], botskill);
            addnbot(clateamsize, teamnames[TEAM_CLA], botskill);
        }

        // play online against other players
        void playonline(const char *name, int team, int weapon, char *servername, int port)
        {
            connectserv(servername, &port, (char*)"");
        }

    } newgame;
};
