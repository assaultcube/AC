// the textview allows showing text and editing text
struct textview : view
{
    int padding, contentwidth, contentheight;
    char *text = 0;
    size_t textstrlen = 0;
    bool editable = false, editing = false;

    enum sizespec
    {
        FIT_CONTENT = 0,
        FILL_PARENT
    } widthspec = FIT_CONTENT, heightspec = FIT_CONTENT;

    textview(view *parent, const char *text, bool editable)
            : view(parent), editable(editable) {
        this->settext(text);
    };

    ~textview()
    {
        DELETEA(text);
    }

    void settext(const char *text)
    {
        DELETEA(this->text);
        // default AC string length is not sufficient for this use case so we use a dynamic string with a minimum size of MAXSTRLEN
        // therefore do *NOT* use string functions from tools.h in this class (!)
        textstrlen = max((size_t)(strlen(text) + 1), (size_t) MAXSTRLEN);
        this->text = new char[textstrlen];
        strncpy(this->text, text, textstrlen);
        this->text[textstrlen-1] = 0;
    }

    void measure(int availablewidth, int availableheight)
    {
        padding = FONTH/2;

        if(widthspec == FILL_PARENT)
        {
            width = availablewidth;
            contentwidth = width-2*padding;
        }
        else
        {
            contentwidth = text_width(text);
            width = contentwidth + padding;
        }

        if(heightspec == FILL_PARENT)
        {
            height = availableheight;
            contentheight = height-2*padding;
        }
        else
        {
            contentheight = FONTH;
            height = contentheight + padding;
        }
    }

    void render(int x, int y)
    {
        if(!visible) return;

        if(editable) blendbox(x, y, x + contentwidth + 2*padding, y + contentheight + 2*padding, false, -1, &config.black);

        glEnable(GL_BLEND);
        draw_text(text, x + padding, y + padding, 255, 255, 255, 255, editable? strlen(text) : -1);
        glDisable(GL_BLEND);

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;
    }

    virtual void bubbleevent(uievent event) {
        view::bubbleevent(event);

        if(editable && event.emitter == this && (event.type == uievent::UIE_TAPPED || event.type == uievent::UIE_TEXTCHANGED))
        {
            // open or close soft keyboard
            if(!SDL_IsTextInputActive())
                SDL_StartTextInput();
            else
                SDL_StopTextInput();

            editing = SDL_IsTextInputActive();

            uievent e;
            e.emitter = this;
            e.type = (SDL_IsTextInputActive() ? uievent::UIE_TEXTSTARTEDIT : uievent::UIE_TEXTENDEDIT);
            view::bubbleevent(e);
        }
    };

    virtual void captureevent(SDL_Event *event)
    {
        view::captureevent(event);

        if(editable)
        {
            switch(event->type) {
                case SDL_TEXTINPUT:
                {
                    size_t used = strlen(text);
                    if(used < textstrlen) copystring(text+used, event->text.text, textstrlen-used);
                    break;
                }
                case SDL_KEYDOWN:
                    switch(event->key.keysym.scancode) {
                        case SDL_SCANCODE_BACKSPACE: {
                            int len = strlen(text);
                            if(len > 0) text[len - 1] = 0;
                            break;
                        }
                        case SDL_SCANCODE_RETURN:
                            focuslost();
                            break;
                        default:
                            break;
                    }

                    break;
            }
        }
    }

    void focuslost()
    {
        if(editing)
        {
            uievent e;
            e.emitter = this;
            e.type = uievent::UIE_TEXTCHANGED;
            e.data = (long long int) text;
            bubbleevent(e);
        }
    }
};