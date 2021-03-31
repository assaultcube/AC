// the settings scene allows to configure a handful of crucial things
struct settingsscene : view
{
    textview *title = NULL;
    navigationbutton *prevbutton = NULL;
    sliderview *pointerspeed = NULL;
    sliderview *pointeracceleration = NULL;
    sliderview *volumeup = NULL;
    sliderview *devmode = NULL;

    settingsscene(view *parent) : view(parent)
    {
        title = new textview(this, "Settings", false);
        children.add(title);

        prevbutton = new navigationbutton(this, navigationbutton::PREV);
        children.add(prevbutton);

        game.settings.load();
        pointerspeed = new sliderview(this, "Pointer Speed", 0, 100, game.settings.pointerspeed, NULL);
        children.add(pointerspeed);
        pointeracceleration = new sliderview(this, "Pointer Acceleration", 0, 100, game.settings.pointeracceleration, NULL);
        children.add(pointeracceleration);

        vector<char*> keys;
        keys.add("DOUBLE TAP");
        keys.add("VOLUME-UP");
        volumeup = new sliderview(this, "Attack Key", 0, 1, game.settings.volumeup, &keys);
        children.add(volumeup);

        vector<char*> onoff;
        onoff.add("OFF");
        onoff.add("ON");
        devmode = new sliderview(this, "Developer Mode", 0, 1, game.settings.devmode, &onoff);
        children.add(devmode);
    };

    ~settingsscene()
    {
        DELETEP(title);
        DELETEP(prevbutton);
        DELETEP(pointerspeed);
        DELETEP(pointeracceleration);
        DELETEP(volumeup);
        DELETEP(devmode);
    }

    virtual void measure(int availablewidth, int availableheight)
    {
        title->measure(availablewidth/4, availableheight/4);
        prevbutton->measure(availablewidth/4, availableheight/4);
        pointerspeed->measure(availablewidth/2, availableheight/4);
        pointeracceleration->measure(availablewidth/2, availableheight/4);
        volumeup->measure(availablewidth/2, availableheight/4);
        devmode->measure(availablewidth/2, availableheight/4);

        pointerspeed->mintextwidth = pointeracceleration->mintextwidth = text_width(pointeracceleration->text);
        volumeup->mintextwidth = devmode->mintextwidth = volumeup->width * 0.8;

        width = availablewidth;
        height = availableheight;
    }

    void render(int x, int y)
    {
        title->render(x + width/2 - title->width/2, y + height/8);

        int itemsize = height/4;
        prevbutton->render(itemsize/2/5, itemsize/2/5);

        pointerspeed->render(x + width/2 - pointerspeed->width/2, title->bbox.y2 + height/8 );
        pointeracceleration->render(x + width/2 - pointerspeed->width/2, title->bbox.y2 + height/8 + pointerspeed->height + FONTH );
        volumeup->render(x + width/2 - pointerspeed->width/2, title->bbox.y2 + height/8 + pointerspeed->height + FONTH + pointeracceleration->height + FONTH);
        devmode->render(x + width/2 - pointerspeed->width/2, title->bbox.y2 + height/8 + pointerspeed->height + FONTH + pointeracceleration->height + FONTH + devmode->height + FONTH);

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
                    game.settings.load();
                    game.settings.pointerspeed = pointerspeed->currentval;
                    game.settings.pointeracceleration = pointeracceleration->currentval;
                    game.settings.volumeup = volumeup->currentval;
                    game.settings.devmode = devmode->currentval;
                    game.settings.save();
                    delete viewstack.pop();
                }
                break;
            default:
                break;
        }
        view::bubbleevent(e);
    }
};