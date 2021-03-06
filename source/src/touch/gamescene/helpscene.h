// the settings scene allows to configure a handful of crucial things
struct helpscene : view
{
    textview *title = NULL;
    navigationbutton *prevbutton = NULL;

    helpscene(view *parent) : view(parent)
    {
        title = new textview(this, "Help", false);
        children.add(title);

        prevbutton = new navigationbutton(this, navigationbutton::PREV);
        children.add(prevbutton);
    };

    ~helpscene()
    {
        DELETEP(title);
        DELETEP(prevbutton);
    }

    virtual void measure(int availablewidth, int availableheight)
    {
        title->measure(availablewidth/4, availableheight/4);
        prevbutton->measure(availablewidth/4, availableheight/4);
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

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;
    }

    virtual void captureevent(SDL_Event *event)
    {
        if(event->type == SDL_FINGERDOWN)
        {
            delete viewstack.pop();
            return;
        }
        view::captureevent(event);
    }
};