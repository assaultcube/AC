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

    virtual ~consolebuffer() {}

    virtual void render() = 0;
};

struct textinputbuffer
{
    string buf;
    int pos;

    textinputbuffer() : pos(-1)
    {
        buf[0] = '\0';
    }

    bool key(int code, bool isdown, int unicode)
    {
        switch(code)
        {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                break;

            case SDLK_HOME: 
                if(strlen(buf)) pos = 0; 
                break; 

            case SDLK_END: 
                pos = -1; 
                break; 

            case SDLK_DELETE:
            {
                int len = (int)strlen(buf);
                if(pos<0) break;
                memmove(&buf[pos], &buf[pos+1], len - pos);
                if(pos>=len-1) pos = -1;
                break;
            }

            case SDLK_BACKSPACE:
            {
                int len = (int)strlen(buf), i = pos>=0 ? pos : len;
                if(i<1) break;
                memmove(&buf[i-1], &buf[i], len - i + 1);
                if(pos>0) pos--;
                else if(!pos && len<=1) pos = -1;
                break;
            }

            case SDLK_LEFT:
                if(pos > 0) pos--;
                else if(pos < 0) pos = (int)strlen(buf)-1;
                break;

            case SDLK_RIGHT:
                if(pos>=0 && ++pos>=(int)strlen(buf)) pos = -1;
                break;

            case SDLK_v:
                extern void pasteconsole(char *dst);
                if(SDL_GetModState()&(KMOD_LCTRL|KMOD_RCTRL))
                {
                    pasteconsole(buf);
                    break;
                }

            default:
            {
                if(unicode)
                {
                    size_t len = strlen(buf);
                    if(len+1 < sizeof(buf))
                    {
                        if(pos < 0) buf[len] = unicode;
                        else 
                        {
                            memmove(&buf[pos+1], &buf[pos], len - pos);
                            buf[pos++] = unicode;
                        }
                        buf[len+1] = '\0';
                        return true;
                    }
                }
                break;
            }
        }
        return false;
    }
};

