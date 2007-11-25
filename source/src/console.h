struct cline { char *cref; int millis; };

struct consolebuffer
{
    vector<cline> conlines;

    void addline(const char *sf, bool highlight, int millis)        // add a line to the console buffer
    {
        cline cl;
        cl.cref = conlines.length()>100 ? conlines.pop().cref : newstringbuf("");   // constrain the buffer size
        cl.millis = millis;                        // for how long to keep line on screen
        conlines.insert(0,cl);
        if(highlight)                                   // show line in a different colour, for chat etc.
        {
            cl.cref[0] = '\f';
            cl.cref[1] = '0';
            cl.cref[2] = 0;
            s_strcat(cl.cref, sf);
        }
        else
        {
            s_strcpy(cl.cref, sf);
        }
    }

    virtual void render() = 0;
};