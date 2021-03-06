// the settings scene allows to configure a handful of crucial things
struct introscene : view
{
    textview *title = NULL;
    navigationbutton *prevbutton = NULL;
    const char *text =
            "AssaultCube has been created and nurtured by an international\n"
            "community of artists and developers since July 2004. We are\n"
            "people who love building fun games.\n\n"
            "\f2We are looking for lead developers to help us build the next\n"
            "generation of AssaultCube for Windows, Mac, Linux and Mobile.\n\n"
            "\f5Enjoy the game!";
    int textwidth = 0;

    introscene(view *parent) : view(parent)
    {
        title = new textview(this, text, false);
        children.add(title);

        prevbutton = new navigationbutton(this, navigationbutton::PREV);
        children.add(prevbutton);
    };

    ~introscene()
    {
        DELETEP(title);
        DELETEP(prevbutton);
    }

    virtual void measure(int availablewidth, int availableheight)
    {
        title->measure(availablewidth/4, availableheight/4);
        prevbutton->measure(availablewidth/4, availableheight/4);
        textwidth = text_width(text);
        width = availablewidth;
        height = availableheight;
    }

    void render(int x, int y)
    {
        int childheights = title->height;
        int yoffset = height/2 - childheights;
        title->render(x + (VIRTW-textwidth)/2, y + yoffset);

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;
    }
};