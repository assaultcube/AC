// the settings scene allows to configure a handful of crucial things
struct settingsscene : view
{
    textview *title = NULL;
    navigationbutton *prevbutton = NULL;
    sliderview *pointerspeed = NULL;
    sliderview *pointeracceleration = NULL;
    sliderview *attackcontrol = NULL;
    sliderview *jumpcontrol = NULL;
    sliderview *devmode = NULL;

    settingsscene(view *parent) : view(parent)
    {
        title = new textview(this, "Settings", false);
        children.add(title);

        prevbutton = new navigationbutton(this, navigationbutton::PREV);
        children.add(prevbutton);

        pointerspeed = new sliderview(this, "Pointer Speed", 0, 100, game.settings.pointerspeed, NULL);
        children.add(pointerspeed);
        pointeracceleration = new sliderview(this, "Pointer Acceleration", 0, 100, game.settings.pointeracceleration, NULL);
        children.add(pointeracceleration);

        vector<char*> onoff;
        onoff.add("OFF");
        onoff.add("ON");
        devmode = new sliderview(this, "Developer Mode", 0, 1, game.settings.devmode, &onoff);
        children.add(devmode);

        vector<char*> jumpcontrols;
        jumpcontrols.add("SWIPE");
        jumpcontrols.add("JUMP BUTTON");
        jumpcontrol = new sliderview(this, "Jump Control", 0, 1, game.settings.jumpcontrol, &jumpcontrols);
        children.add(jumpcontrol);

        vector<char*> attackcontrols;
        attackcontrols.add("DOUBLE TAP");
        attackcontrols.add("VOLUME-UP KEY");
        attackcontrols.add("ATTACK BUTTON");
        attackcontrol = new sliderview(this, "\f3Try it out! \f5Attack/fire Control", 0, 2, game.settings.attackcontrol, &attackcontrols);
        children.add(attackcontrol);
    };

    ~settingsscene()
    {
        DELETEP(title);
        DELETEP(prevbutton);
        DELETEP(pointerspeed);
        DELETEP(pointeracceleration);
        DELETEP(devmode);
        DELETEP(jumpcontrol);
        DELETEP(attackcontrol);
    }

    virtual void measure(int availablewidth, int availableheight)
    {
        title->measure(availablewidth/4, availableheight/4);
        prevbutton->measure(availablewidth/4, availableheight/4);
        pointerspeed->measure(availablewidth/2, availableheight/4);
        pointeracceleration->measure(availablewidth/2, availableheight/4);
        devmode->measure(availablewidth/2, availableheight/4);
        jumpcontrol->measure(availablewidth / 2, availableheight / 4);
        attackcontrol->measure(availablewidth / 2, availableheight / 4);

        pointerspeed->mintextwidth = pointeracceleration->mintextwidth = text_width(pointeracceleration->text);
        devmode->mintextwidth = jumpcontrol->mintextwidth = attackcontrol->width * 0.85;
        attackcontrol->mintextwidth = attackcontrol->width * 0.75;

        width = availablewidth;
        height = availableheight;
    }

    void render(int x, int y)
    {
        title->render(x + width/2 - title->width/2, y + height/8);

        int itemsize = height/4;
        prevbutton->render(itemsize/2/5, itemsize/2/5);
        pointerspeed->render(x + width/2 - pointerspeed->width/2, title->bbox.y2 + height/8 );
        pointeracceleration->render(x + width/2 - pointerspeed->width/2, pointerspeed->bbox.y2 + FONTH );
        devmode->render(x + width/2 - pointerspeed->width/2, pointeracceleration->bbox.y2 + FONTH);
        jumpcontrol->render(x + width / 2 - pointerspeed->width / 2, devmode->bbox.y2 + FONTH);
        attackcontrol->render(x + width / 2 - pointerspeed->width / 2, jumpcontrol->bbox.y2 + FONTH);

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
                    game.settings.pointerspeed = pointerspeed->currentval;
                    game.settings.pointeracceleration = pointeracceleration->currentval;
                    game.settings.devmode = devmode->currentval;
                    game.settings.jumpcontrol = game::settings::jumpcontroltype(jumpcontrol->currentval);
                    game.settings.attackcontrol = game::settings::attackcontroltype(attackcontrol->currentval);
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