// the UI event is emitted by a target view and bubbles from the innermost view up to the outermost view until it is handled
struct uievent
{
    view *emitter; // the view that raised the UI event

    enum uieventtype // short and sweet global set of known events keeps away the necessity of managing individual callbacks per view
    {
        UIE_TAPPED = 0, // data=0
        UIE_SELECTED, // data=key of selected menu item
        UIE_TEXTSTARTEDIT,
        UIE_TEXTENDEDIT,
        UIE_TEXTCHANGED // data= char* pointer to changed text
    };
    uieventtype type;

    long long int data; // additional data provided by the target such as the key of the selected menu item
};

// the view is the unit that receives captured events, bubbles events and renders content. views are organized within a view hierarchy.
struct view
{
    int width = -1, height = -1;
    view *parent;
    vector<view *> children;
    bool visible = true, hasevents = true;

    struct boundingbox
    {
        int x1, x2, y1, y2;
    } bbox;

    view(view *parent) : parent(parent)
    {
    }
    virtual ~view() {}

    virtual void oncreate() { loopv(children) children[i]->oncreate(); }; // called after view hierarchy completion and before measure
    virtual void measure(int availablewidth, int availableheight) = 0; // called after oncreate and before render
    virtual void render(int x, int y) = 0; // called after measure
    virtual void focuslost() { loopv(children) children[i]->focuslost(); } // called after focus has been lost

    // send captured events from outermost view down to innermost view until target is found
    virtual void captureevent(SDL_Event *event)
    {
        switch(event->type)
        {
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            case SDL_TEXTINPUT:
            case SDL_TEXTEDITING:
                loopv(children)
                {
                    if(!children[i]->hasevents) continue;
                    if(!children[i]->visible) continue;
                    children[i]->captureevent(event);
                }
                break;

            case SDL_FINGERDOWN:
            case SDL_FINGERUP:
            case SDL_FINGERMOTION:
                int x = event->tfinger.x * VIRTW;
                int y = event->tfinger.y * VIRTH;
                loopv(children)
                {
                    if(!children[i]->hasevents) continue;
                    if(!children[i]->visible) continue;
                    if(x >= children[i]->bbox.x1
                       && x <= children[i]->bbox.x2
                       && y >= children[i]->bbox.y1
                       && y <= children[i]->bbox.y2)
                    {
                        children[i]->captureevent(event);
                        return;
                    }
                }
                if(event->type == SDL_FINGERDOWN)
                {
                    uievent uievent;
                    uievent.emitter = this;
                    uievent.type = uievent.UIE_TAPPED;
                    bubbleevent(uievent);
                }
                return;
        }
    }

    // bubble events from innermost view up to outermost view until handled
    virtual void bubbleevent(uievent event) {
        if(parent) parent->bubbleevent(event);
    };

    // renders the view hierarchy with this view being the root node
    void renderroot()
    {
        setscope(false);

        glPushMatrix();
        glEnable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        glDisable(GL_DEPTH_TEST);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, VIRTW, VIRTH, 0, -1, 1);

        // blur background
        color c(1.0f, 1.0f, 1.0f, 0.2f);
        blendbox(0.0f, 0.0f, VIRTW, VIRTH, false, -1, &c);
        glBlendFunc(GL_ONE, GL_ZERO);
        glColor3f(1.0, 1.0, 1.0);

        measure(VIRTW, VIRTH);
        render(0, 0);

        glDisable(GL_BLEND);
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_DEPTH_TEST);
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }
};

