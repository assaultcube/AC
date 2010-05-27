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
        copystring(cl.line, sf);
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

/** This is the 1.0.4 structure
    It will substituted by textinputbuffer_wip
    I am putting this temporarily here to fix the weird cursor behavior
    Please, read the comments in rendercommand::console.cpp : Brahma */
struct textinputbuffer
{
    string buf;
    int pos, max;

    textinputbuffer() : pos(-1), max(0)
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
#ifdef __APPLE__
#define MOD_KEYS (KMOD_LMETA|KMOD_RMETA) 
#else
#define MOD_KEYS (KMOD_LCTRL|KMOD_RCTRL)
#endif
                if(SDL_GetModState()&MOD_KEYS)
                {
                    pasteconsole(buf);
                    return true;
                }
                // fall through

            default:
            {
                if(unicode)
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

/** WIP ALERT */
struct textinputbuffer_wip
{
    string buf;
    int pos, max;

    textinputbuffer_wip() : pos(-1), max(0)
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
                if(pos < 0) break;

                std::string s(buf);
                int len = utf8::distance(s.begin(), s.end());
                std::string::iterator first = s.begin();
                std::string::iterator second = s.begin();
                utf8::advance(first, pos, s.end());
                utf8::advance(second, pos + 1, s.end());
                s.erase(first, second);
                strcpy(buf, s.c_str());

                if(pos>=len-1) pos = -1;
                return true;
            }

            case SDLK_BACKSPACE:
            {
                std::string s(buf);
                int len = utf8::distance(s.begin(), s.end());
                int i = pos>=0 ? pos : len;
                if(i<1) break;

                std::string::iterator first = s.begin();
                std::string::iterator second = s.begin();
                utf8::advance(first, i - 1, s.end());
                utf8::advance(second, i, s.end());
                s.erase(first, second);
                strcpy(buf, s.c_str());

                if(pos > 0) pos--;
                else if(!pos && len <=1 ) pos = -1;
                return true;
            }

            case SDLK_LEFT:
            {
                std::string s(buf);
                int len = utf8::distance(s.begin(), s.end());

                if(pos > 0) pos--;
                else if(pos < 0) pos = len-1;
                break;
            }

            case SDLK_RIGHT:
            {
                std::string s(buf);
                int len = utf8::distance(s.begin(), s.end());
                if(pos>=0 && ++pos >= len) pos = -1;
                break;
            }

            case SDLK_v:
                extern void pasteconsole(char *dst);
#ifdef __APPLE__
#define MOD_KEYS (KMOD_LMETA|KMOD_RMETA) 
#else
#define MOD_KEYS (KMOD_LCTRL|KMOD_RCTRL)
#endif
                if(SDL_GetModState()&MOD_KEYS)
                {
                    pasteconsole(buf);
                    return true;
                }
                // fall through

            default:
            {
                if(unicode)
                {
                    std::string s(buf);
                    char tmp[5] = { 0, 0, 0, 0, 0 };
                    utf8::append(unicode, tmp);
                    std::string newchar(tmp);

                    int len = utf8::distance(s.begin(), s.end());
                    if(max && (int)len>=max) break;

                    if(s.length() + newchar.length() < sizeof(buf))
                    {
                        if(pos < 0)
                        {
                            s.append(newchar);
                        }
                        else
                        {
                            std::string::iterator first = s.begin();
                            utf8::advance(first, pos + 1, s.end());
                            s.insert(first, newchar.begin(), newchar.end());
                            pos++;
                        }
                        strcpy(buf, s.c_str());
                        return true;
                    }
                }
                break;
            }
        }
        return false;
    }
};

