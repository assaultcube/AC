// the game scene allows changing gameplay relevant settings while staying the game
struct gamescene : view
{
    textview *title = NULL;
    navigationbutton *prevbutton = NULL;
    imagetouchmenuitem *profilebutton = NULL;
    imagetouchmenuitem *settingsbutton = NULL;
    imagetouchmenuitem *helpbutton = NULL;
    imagetouchmenuitem *disconnectbutton = NULL;
    imagetouchmenuitem *creditsbutton = NULL;

    gamescene(view *parent) : view(parent)
    {
        title = new textview(this, "Game Menu", false);
        children.add(title);

        prevbutton = new navigationbutton(this, navigationbutton::PREV);
        children.add(prevbutton);

        profilebutton = new imagetouchmenuitem(this, 0, "packages/misc/touch.png", 1.0/4.0f,1.0/4.0f, 2, 2);
        profilebutton->circleborder = true;
        children.add(profilebutton);

        settingsbutton = new imagetouchmenuitem(this, 0, "packages/misc/touch.png", 1.0/4.0f,1.0/4.0f, 2, 0);
        settingsbutton->circleborder = true;
        children.add(settingsbutton);

        helpbutton = new imagetouchmenuitem(this, 0, "packages/misc/touch.png", 1.0/4.0f,1.0/4.0f, 2, 3);
        helpbutton->circleborder = true;
        children.add(helpbutton);

        disconnectbutton = new imagetouchmenuitem(this, 0, "packages/misc/touch.png", 1.0/4.0f,1.0/4.0f, 2, 1);
        disconnectbutton->circleborder = true;
        children.add(disconnectbutton);

        creditsbutton = new imagetouchmenuitem(this, 0, "packages/misc/touch.png", 1.0/4.0f,1.0/4.0f, 3, 0);
        creditsbutton->circleborder = true;
        children.add(creditsbutton);
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
    }

    virtual void measure(int availablewidth, int availableheight)
    {
        title->measure(availableheight, availableheight);
        prevbutton->measure(availableheight/4, availableheight/4);
        profilebutton->measure(availablewidth/4, availableheight/4);
        settingsbutton->measure(availablewidth/4, availableheight/4);
        helpbutton->measure(availablewidth/4, availableheight/4);
        disconnectbutton->measure(availablewidth/4, availableheight/4);
        creditsbutton->measure(availablewidth/4, availableheight/4);
        width = availablewidth;
        height = availableheight;
    }

    void render(int x, int y)
    {
        title->render(x + width/2 - text_width(title->text)/2, y + height/8);

        int itemsize = height/4;
        prevbutton->render(itemsize/2/5, itemsize/2/5);
        profilebutton->render(x + width - 2*profilebutton->width - itemsize/2/5, VIRTH-itemsize/2/5-itemsize-settingsbutton->height);
        settingsbutton->render(x + width - 2*settingsbutton->width - itemsize/2/5, VIRTH-itemsize/2/5-itemsize);
        helpbutton->render(x + width - helpbutton->width - itemsize/2/5, VIRTH-itemsize/2/5-itemsize-disconnectbutton->height);
        disconnectbutton->render(VIRTW - disconnectbutton->width - itemsize/2/5, VIRTH-itemsize/2/5-itemsize);
        creditsbutton->render(itemsize/2/5, VIRTH-itemsize/2/5-itemsize);

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
                break;
            case uievent::UIE_SELECTED:
                break;
            default:
                break;
        }
        view::bubbleevent(e);
    }
};