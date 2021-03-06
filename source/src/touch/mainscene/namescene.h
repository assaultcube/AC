// the skin scene allows choosing the skin
struct namescene : view
{
    textview *title;
    textview *name;
    string nametxt;
    bool snaptop = false;

    namescene(view *parent) : view(parent)
    {
        copystring(nametxt, player1->name);

        title = new textview(this, "Your name", false);

        name = new textview(this, nametxt, true);
        name->widthspec = textview::FILL_PARENT;
        name->heightspec = textview::FIT_CONTENT;

        children.add(name);
    };

    ~namescene()
    {
        DELETEP(title);
        DELETEP(name);
    }

    virtual void measure(int availablewidth, int availableheight)
    {
        title->measure(availablewidth, availableheight);
        name->measure(text_width(title->text) * 3, availableheight);
        width = availablewidth;
        height = availableheight;
    }

    void render(int x, int y)
    {
        int titlewidth = text_width(title->text);
        int childheights = title->height + name->height;
        int itemsize = VIRTH/8;
        int yoffset = snaptop ? (itemsize - title->height - name->height/2) : height/2 - childheights;
        title->render(x + width/2 - titlewidth/2*3, y + yoffset);
        name->render(x + width/2 - titlewidth/2*3, y + yoffset + 2*FONTH);

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;
    }

    virtual void bubbleevent(uievent e)
    {
        if(e.emitter == name && e.type == uievent::UIE_TEXTCHANGED)
        {
            copystring(nametxt, (char*)e.data);

            // immediately apply name
            extern void newname(const char *name);
            newname((strlen(nametxt) > 0) ? nametxt : "unarmed");
        }
        else if(e.emitter == name && e.type == uievent::UIE_TEXTSTARTEDIT)
        {
            snaptop = true;
        }
        else if(e.emitter == name && e.type == uievent::UIE_TEXTENDEDIT)
        {
            snaptop = false;
        }
        view::bubbleevent(e);
    }
};
