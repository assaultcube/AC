struct votedisplayinfo
{
    playerent *owner;
    int type, stats[VOTE_NUM], result, millis;
    string desc;
    votedisplayinfo() : owner(NULL), result(VOTE_NEUTRAL), millis(0) { loopi(VOTE_NUM) stats[i] = VOTE_NEUTRAL; }
};