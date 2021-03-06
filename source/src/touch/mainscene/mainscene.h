// the main scene is shown when the game starts
struct mainscene : view
{
    navigationbutton *prevbutton = NULL;
    navigationbutton *nextbutton = NULL;
    imagetouchmenuitem *okbutton = NULL;

    introscene *intro = NULL;
    namescene *name = NULL;
    weaponscene *weapons = NULL;
    teamscene *teams = NULL;
    skinscene *skins = NULL;
    serverscene *servers = NULL;
    int weapon, team, skin;

    vector<view *> scenes;
    view *currentscene = NULL;
    view *previousscene = NULL;
    int currentsceneidx = -1;
    int animmillis = 0;
    int animdirection = 0;
    const int animdurationmillis = 500;
    view *focused = NULL;

    mainscene(view *parent) : view(parent)
    {
        prevbutton = new navigationbutton(this, navigationbutton::PREV);
        children.add(prevbutton);
        nextbutton = new navigationbutton(this, navigationbutton::NEXT);
        children.add(nextbutton);
        okbutton = new imagetouchmenuitem(this, 0, "packages/misc/touch.png", 1.0/4.0f,1.0/4.0f, 1, 1);
        okbutton->circleborder = true;
        children.add(okbutton);

        // order of scenes visible to the user
        scenes.add(intro = new introscene(this));
        scenes.add(name = new namescene(this));
        scenes.add(weapons = new weaponscene(this));
        scenes.add(teams = new teamscene(this));
        scenes.add(skins = new skinscene(this));
        scenes.add(servers = new serverscene(this));

        // order of screens when being processed by oncreate(..), measure(..) and render(..)
        children.add(intro);
        children.add(name);
        children.add(skins); // weapons and teams will emit events during oncreate(..) so we want skins to be already listening when that happens
        children.add(weapons);
        children.add(teams);
        children.add(servers);

        setcurrentscene(0);
    };

    ~mainscene()
    {
        DELETEP(prevbutton);
        DELETEP(nextbutton);
        DELETEP(okbutton);
        DELETEP(name);
        DELETEP(intro);
        DELETEP(weapons);
        DELETEP(teams);
        DELETEP(skins);
        DELETEP(servers);
    }

    void showserverscene() { setcurrentscene(5); }

    virtual void measure(int availablewidth, int availableheight)
    {
        prevbutton->measure(availableheight/4, availableheight/4);
        nextbutton->measure(availableheight/4, availableheight/4);
        okbutton->measure(availableheight/4, availableheight/4);
        if(currentscene) currentscene->measure(availablewidth, availableheight);
        width = availablewidth;
        height = availableheight;
    }

    void render(int x, int y)
    {
        // move this to a better place
        static bool firstrender = true;
        if(firstrender)
        {
            audiomgr.music("pingpong/Ping_Pong_-_Kamikadze.ogg", 89000, "");
            firstrender = false;
        }

        float animprogress = min(animdurationmillis, lastmillis - animmillis) / (float) animdurationmillis;
        float animation = sinf(animprogress * PI/2.0f);
        float xoffset = (width - (animation * width)) * (animdirection > 0 ? 1 : -1);
        if(currentscene) currentscene->render(xoffset, 0);
        if(previousscene && animprogress < 1.0f) previousscene->render(xoffset - (animdirection > 0 ? width : -width), 0);

        int itemsize = height/4;
        prevbutton->render(itemsize/2/5, itemsize/2/5);
        nextbutton->render(VIRTW-nextbutton->width-itemsize/2/5, itemsize/2/5);
        okbutton->render(VIRTW-nextbutton->width-itemsize/2/5, itemsize/2/5);

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
                if(e.emitter == nextbutton) navigate(1);
                else if(e.emitter == prevbutton) navigate(-1);
                else if(e.emitter == okbutton)
                {
                    if(servers->menu->selection >= 0 && servers->menu->selection < servers->menu->items.length())
                    {
                        servermenuitem *mitem = (servermenuitem*)servers->menu->items[servers->menu->selection];
                        int teamid = (team == teamscene::CLA ? TEAM_CLA : TEAM_RVSF);
                        bool showhelpscene = false;

                        switch(mitem->battlegroundtype)
                        {
                            case servermenuitem::BG_TRAINING:
                            {
                                game.newgame.training(name->nametxt, teamid, weapon, mitem->map);
                                showhelpscene = true;
                                break;
                            }
                            case servermenuitem::BG_PLAYOFFLINE:
                            {
                                game::newgame::difficulty d;
                                int playofflineindex = servers->menu->selection - servers->playofflineoffset;
                                switch(playofflineindex)
                                {
                                    case 0: d = game::newgame::difficulty::EASY; break;
                                    case 1: d = game::newgame::difficulty::NORMAL; break;
                                    default: d = game::newgame::difficulty::HARD; break;
                                }
                                game.newgame.playoffline(name->nametxt, teamid, weapon, mitem->map, d);
                                break;
                            }
                            case servermenuitem::BG_PLAYONLINE:
                                game.newgame.playonline(name->nametxt, teamid, weapon, servers->selectedserver.name, servers->selectedserver.port);
                                break;
                        }

                        writecfg();
                        viewstack.deletecontents();

                        if(showhelpscene)
                        {
                            view *help = new helpscene(this);
                            help->oncreate();
                            viewstack.add(help);
                        }
                    }
                }
                break;
            case uievent::UIE_SELECTED:
                if(e.emitter == weapons)
                {
                    weapon = e.data;
                    skins->weapon = weapon;
                    skins->updatemodel();
                }
                else if(e.emitter == teams)
                {
                    team = e.data;
                    skins->team = (team == teamscene::CLA ? skinscene::CLA : skinscene::RVSF);
                    skins->updatemodel();
                }
                break;
            case uievent::UIE_TEXTSTARTEDIT:
                focused = e.emitter;
                break;
            case uievent::UIE_TEXTENDEDIT:
                focused = NULL;
                updatebuttonvisibility();
                break;
            default:
                break;
        }
        view::bubbleevent(e);
    }

    void navigate(int direction)
    {
        if(scenes.length() == 0) return;
        animmillis = lastmillis;
        animdirection = direction > 0 ? 1 : -1;

        int idx = -1, nextidx = -1;
        loopv(scenes) if(scenes[i] == currentscene) { idx = i; break; }

        if(idx >= 0)
            nextidx = (direction > 0 ? idx+1 : idx+scenes.length()-1) % scenes.length();
        else
            nextidx = 0;

        previousscene = currentscene;
        setcurrentscene(nextidx);
        audiomgr.playsoundc(S_MENUSELECT, player1, SP_HIGHEST);
    }

    void setcurrentscene(int nextidx)
    {
        currentscene = scenes[nextidx];
        currentsceneidx = nextidx;
        loopv(scenes)
        {
            scenes[i]->hasevents = (i == nextidx);
            if(i != nextidx) scenes[i]->focuslost();
        }
        updatebuttonvisibility();
        if(nextidx == scenes.length() - 1) onboarded = 1;
    }

    void updatebuttonvisibility()
    {
        prevbutton->visible = currentsceneidx > 0;
        nextbutton->visible = (currentsceneidx != scenes.length() - 1) && (name != currentscene || strlen(name->nametxt) > 0);
        okbutton->visible = (currentsceneidx == scenes.length() - 1);
    }
};


