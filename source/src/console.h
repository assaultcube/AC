struct cline { char *line; int millis; };

template<class LINE> struct consolebuffer
{
    int maxlines;
    vector<LINE> conlines;

    consolebuffer(int maxlines = 100) : maxlines(maxlines) {}

    LINE &addline(const char *sf, int millis)        // add a line to the console buffer
    {
        LINE cl;
        cl.line = conlines.length()>maxlines ? conlines.pop().line : newstringbuf("");   // constrain the buffer size
        cl.millis = millis;                        // for how long to keep line on screen
        extern void encodeigraphs(char *d, const char *s, int len);
        encodeigraphs(cl.line, sf, MAXSTRLEN);
        return conlines.insert(0, cl);
    }

    void setmaxlines(int numlines)
    {
        maxlines = numlines;
        while(conlines.length() > maxlines) delete[] conlines.pop().line;
    }

    virtual ~consolebuffer()
    {
        while(conlines.length()) delete[] conlines.pop().line;
    }

    virtual void render() = 0;
};

struct textinputbuffer
{
    string buf;
    int pos, max;

    textinputbuffer() : pos(-1), max(0)
    {
        buf[0] = '\0';
    }

    bool key(int code, bool isdown, int unicode)    // returns true if buffer was modified
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
                return true;
            }

            case SDLK_BACKSPACE:
            {
                int len = (int)strlen(buf), i = pos>=0 ? pos : len;
                if(i<1) break;
                memmove(&buf[i-1], &buf[i], len - i + 1);
                if(pos>0) pos--;
                else if(!pos && len<=1) pos = -1;
                return true;
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
                if(SDL_GetModState() & MOD_KEYS_CTRL)
                {
                    pasteconsole(buf);
                    return true;
                }
                // fall through

            default:
            {
                extern bool filterunrenderables(char *s);
                char tmp[2] = { (char)unicode, 0 };
                if(unicode && !filterunrenderables(tmp))
                {
                    size_t len = strlen(buf);
                    if(max && (int)len>=max) break;
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

