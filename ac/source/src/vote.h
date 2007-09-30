struct votedisplayinfo
{
    playerent *owner;
    int type, stats[VOTE_NUM], result, millis;
    string desc;
    bool localplayervoted;
    votedisplayinfo() : owner(NULL), result(VOTE_NEUTRAL), millis(0), localplayervoted(false) { loopi(VOTE_NUM) stats[i] = VOTE_NEUTRAL; }
};