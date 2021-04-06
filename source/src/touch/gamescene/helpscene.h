// the settings scene allows to configure a handful of crucial things
struct helpscene : view
{
    imagetouchmenuitem *okbutton = NULL;

    helpscene(view *parent, bool showokbutton) : view(parent)
    {
        if(showokbutton)
        {
            okbutton = new imagetouchmenuitem(this, 0, "packages/misc/touch.png", config.TOUCHICONGRIDCELL, config.TOUCHICONGRIDCELL, 1, 1);
            okbutton->circleborder = true;
            children.add(okbutton);
        }
    };

    ~helpscene()
    {
        DELETEP(okbutton);
    }

    virtual void measure(int availablewidth, int availableheight)
    {
        if(okbutton) okbutton->measure(availablewidth/4, availableheight/4);
        width = availablewidth;
        height = availableheight;
    }

    void render(int x, int y)
    {
        blendbox(0, 0, VIRTW, VIRTH, false, -1, &config.white);

        int imgheight = VIRTH-2*FONTH;
        int imgwidth = imgheight*1024/551;
        static Texture *tex = NULL;
        const char *texname = "packages/misc/touchexplainer.jpg";
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        if(!tex) tex = textureload(texname, 3);
        if(tex) rect(tex->id, x + (VIRTW-imgwidth)/2, y + (VIRTH-imgheight)/2, imgwidth, imgheight, 0.0f, 0.0f, 1.0f, 1.0f);

        int itemsize = height/4;
        if(okbutton) okbutton->render(VIRTW-okbutton->width-itemsize/2/5, VIRTH/2-itemsize/2);

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;
    }

    virtual void captureevent(SDL_Event *event)
    {
        // if there is no OK button we simply treat ever touch of the screen as a close action
        if(!okbutton && event->type == SDL_FINGERDOWN)
        {
            close();
            return;
        }
        view::captureevent(event);
    }

    virtual void bubbleevent(uievent e)
    {
        // if there is an OK button the user is required to tap it in order to close the screen
        if(okbutton && e.type == uievent::UIE_TAPPED && e.emitter == okbutton) close();
    }

    void close()
    {
        delete viewstack.pop();
    }
};