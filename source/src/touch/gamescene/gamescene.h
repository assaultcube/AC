// the game scene allows changing gameplay relevant settings while staying in the game
struct gamescene : view
{
    textview *title = NULL;
    navigationbutton *prevbutton = NULL;
    imagetouchmenuitem *profilebutton = NULL;
    imagetouchmenuitem *settingsbutton = NULL;
    imagetouchmenuitem *helpbutton = NULL;
    imagetouchmenuitem *disconnectbutton = NULL;
    imagetouchmenuitem *creditsbutton = NULL;
    imagetouchmenuitem *consolebutton = NULL;

    gamescene(view *parent) : view(parent)
    {
        title = new textview(this, "Game Menu", false);
        children.add(title);

        prevbutton = new navigationbutton(this, navigationbutton::PREV);
        children.add(prevbutton);

        profilebutton = new imagetouchmenuitem(this, 0, "packages/misc/touch.png", config.TOUCHICONGRIDCELL, config.TOUCHICONGRIDCELL, 2, 2);
        profilebutton->circleborder = true;
        children.add(profilebutton);

        settingsbutton = new imagetouchmenuitem(this, 0, "packages/misc/touch.png", config.TOUCHICONGRIDCELL, config.TOUCHICONGRIDCELL, 0, 2);
        settingsbutton->circleborder = true;
        children.add(settingsbutton);

        helpbutton = new imagetouchmenuitem(this, 0, "packages/misc/touch.png", config.TOUCHICONGRIDCELL, config.TOUCHICONGRIDCELL, 3, 2);
        helpbutton->circleborder = true;
        children.add(helpbutton);

        disconnectbutton = new imagetouchmenuitem(this, 0, "packages/misc/touch.png", config.TOUCHICONGRIDCELL, config.TOUCHICONGRIDCELL, 1, 2);
        disconnectbutton->circleborder = true;
        children.add(disconnectbutton);

        creditsbutton = new imagetouchmenuitem(this, 0, "packages/misc/touch.png", config.TOUCHICONGRIDCELL, config.TOUCHICONGRIDCELL, 0, 3);
        creditsbutton->circleborder = true;
        children.add(creditsbutton);

        // only in developermode: TODO: fullconsole should render larger, meaning readable
        consolebutton = new imagetouchmenuitem(this, 0, "packages/misc/touch.png", config.TOUCHICONGRIDCELL, config.TOUCHICONGRIDCELL, 0, 1 );
        consolebutton->circleborder = true;
        children.add(consolebutton);
    };

    ~gamescene()
    {
        DELETEP(title);
        DELETEP(prevbutton);
        DELETEP(profilebutton);
        DELETEP(settingsbutton);
        DELETEP(helpbutton);
        DELETEP(disconnectbutton);
        DELETEP(creditsbutton);
        DELETEP(consolebutton);
    }

    virtual void measure(int availablewidth, int availableheight)
    {
        int quarterWidth = availablewidth/4;
        int quarterHeight = availableheight/4;
        title->measure(availablewidth, availableheight);
        prevbutton->measure(quarterWidth, quarterHeight);
        profilebutton->measure(quarterWidth, quarterHeight);
        settingsbutton->measure(quarterWidth, quarterHeight);
        helpbutton->measure(quarterWidth, quarterHeight);
        disconnectbutton->measure(quarterWidth, quarterHeight);
        creditsbutton->measure(quarterWidth, quarterHeight);
        consolebutton->measure(quarterWidth, quarterHeight);
        width = availablewidth;
        height = availableheight;
    }

    void render(int x, int y)
    {
        title->render(x + width/2 - text_width(title->text)/2, y + height/8);

        int itemsize = height/4;
        int tenthItemsize = itemsize/2/5;
        int xoffset = x + width - tenthItemsize;
        int yoffset = VIRTH - tenthItemsize - itemsize;
        prevbutton->render(tenthItemsize, tenthItemsize);
        profilebutton->render(xoffset - 2*profilebutton->width, yoffset - settingsbutton->height);
        settingsbutton->render(xoffset - 2*settingsbutton->width, yoffset);
        helpbutton->render(xoffset - helpbutton->width, yoffset-disconnectbutton->height);
        disconnectbutton->render(VIRTW - disconnectbutton->width - tenthItemsize, yoffset);
        creditsbutton->render(tenthItemsize, yoffset);
        if(game.settings.devmode && config.CONSOLEBUTTON) consolebutton->render(3 * tenthItemsize + itemsize, yoffset);

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;
    }

    virtual void bubbleevent(uievent e)
    {
        switch(e.type)
        {
            case uievent::UIE_TAPPED:
                if(e.emitter == prevbutton)
                {
                    viewstack.deletecontents();
                }
                else if(e.emitter == profilebutton)
                {
                    view *newview = new equipmentscene(this);
                    newview->oncreate();
                    viewstack.add(newview);
                }
                else if(e.emitter == settingsbutton)
                {
                    view *newview = new settingsscene(this);
                    newview->oncreate();
                    viewstack.add(newview);
                }
                else if(e.emitter == helpbutton)
                {
                    view *newview = new helpscene(this, false);
                    newview->oncreate();
                    viewstack.add(newview);
                }
                else if(e.emitter == disconnectbutton)
                {
                    trydisconnect();

                    extern void kickallbots();
                    kickallbots();

                    callvote(SA_MAP, config.DEFAULT_MAP, "7", "10");

                    viewstack.deletecontents();
                    showtouchmenu(true);
                    return;
                }
                else if(e.emitter == creditsbutton)
                {
                    view *newview = new creditscene(this);
                    newview->oncreate();
                    viewstack.add(newview);
                }
                else if(e.emitter == consolebutton)
                {
                    game.interaction.triggertoggleconsole();
                    viewstack.deletecontents();
                }
                break;
            case uievent::UIE_SELECTED:
                break;
            default:
                break;
        }
        view::bubbleevent(e);
    }
};