// constants used in touch UI
struct config
{
    // determines if the touch UI is enabled or not, if enabled it will replace classic menus, HUD and input handling
    const bool enabled =
#ifdef __ANDROID__
    true;
#else
    false;
#endif

    color black = color(0.0f, 0.0f, 0.0f);
    color white = color(1.0f, 1.0f, 1.0f);
    color selectedcolor = color(1.0f, 1.0f, 1.0f); // determines the border color for selected menu items
    vec movementcontrolcenter() { return vec(VIRTW/6, VIRTH/2, 0.0f); }; // determines the center position of the movement control (VIRTW is not known at compile time)
    const int HUD_ICONSIZE = 120;
    const float TOUCHICONGRIDCELL = 1/5.0f; // N x N icon grid => 1/N
    enum
    {
        FOUR_DIRECTIONS = 0,
        EIGHT_DIRECTIONS
    } const MOVEMENTDIRECTIONS = FOUR_DIRECTIONS; // determines how many directions the movement control supports, 8 directions means you can strafe-run
    const int DOUBLE_TAP_MILLIS = 200; // determines the maximal amount of millis between two taps to identify the action as double-tap
    const char *DEFAULT_MAP = "ac_complex"; // determines the map to load when disconnecting
    bool volumeupattack = false; // determines if the volume-up key binds to attack
    const char *TRAINING_MAP = "ac_desert"; // determines the map that is used for training
    const int PLAYOFFLINE_GAMEMODE = GMODE_BOTTEAMDEATHMATCH; // determines the game mode when playing offline
    const int PLAYOFFLINE_TIMELIMIT = -1; // determines the time limit when playing offline where -1 means default time limit
    const int PLAYOFFLINE_TEAMSIZE = 3; // determines the team size when playing offline
    const static int PLAYOFFLINE_NUM_DIFFICULTIES = 3;
    const char *PLAYOFFLINE_DIFFICULTIES[PLAYOFFLINE_NUM_DIFFICULTIES] = { "EASY", "NORMAL", "HARD" }; // determines the labels for difficulty levels
    const char *PLAYOFFLINE_BOTSKILLS[PLAYOFFLINE_NUM_DIFFICULTIES] = { "bad", "medium", "good" }; // determines the bot skill for each difficulty level
    const int PLAYONLINE_MINSERVERS = 4; // determines the minimum amount of servers to show to the user
    const bool UPDATEFROMMASTER = false; // determines if the serverlist should be update from master. please keep this disabled because this is done at app startup in Java world

    // supported maps for mobile must qualify as follows:
    // * the map must be an official map to ensure quality
    // * the map must be sufficiently bright to ensure that enemies are well visible on average mobile devices (tested with Samsung S7 at 100% screen brightness - no gamma tweaks allowed)
    // * the map must have wide spaces so that mobile players do not get stuck in walls easily
    // * the map must not have dead-ends so that mobile players can navigate without 180° turns
    // * the map must support a team-based mode
    // * the map must have good quality bot waypoints
    const static int NUM_SUPPORTEDMAPS = 9;
    const char *SUPPORTEDMAPS[NUM_SUPPORTEDMAPS] = {
            "ac_arctic",
            "ac_complex",
            "ac_desert",
            "ac_desert2",
            "ac_douze",
            "ac_power",
            "ac_sunset",
            "ac_terros",
            "ac_venison" };

};