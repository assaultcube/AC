// the team team allows choosing the team to join
struct teamscene : view
{
    textview *title;
    textview *pick;
    modelview *teammate1, *teammate2, *teammate3, *teammate4, *flag;
    touchmenu *menu;
    char weaponmdls[4][MAXSTRLEN];

    enum teamtype : int { CLA = 0, RVSF };
    teamtype team = CLA;

    teamscene(view *parent) : view(parent)
    {
        title = new textview(this, "Preferred team", false);
        pick = new textview(this, "", false);
        children.add(title);
        children.add(pick);

        menu = new touchmenu(this);
        menu->rows = 1, menu->cols = 2;
        menu->items.add(new imagetouchmenuitem(menu, RVSF, "packages/misc/teams.png", 1.0 / 2.0f, 1.0f, 1, 0));
        menu->items.add(new imagetouchmenuitem(menu, CLA, "packages/misc/teams.png", 1.0 / 2.0f, 1.0f, 0, 0));
        menu->children.add(menu->items[0]);
        menu->children.add(menu->items[1]);
        children.add(menu);

        teammate1 = new modelview(this);
        teammate2 = new modelview(this);
        teammate3 = new modelview(this);
        teammate4 = new modelview(this);
        children.add(teammate1);
        children.add( teammate2);
        children.add(teammate3);
        children.add(teammate4);

        flag = new modelview(this);
        children.add(flag);
    };

    ~teamscene()
    {
        DELETEP(title);
        DELETEP(pick);
        DELETEP(teammate1);
        DELETEP(teammate2);
        DELETEP(teammate3);
        DELETEP(teammate4);
        DELETEP(flag);
        DELETEP(menu);
    }

    virtual void oncreate() {
        view::oncreate();
        menu->select(0);
    };

    virtual void measure(int availablewidth, int availableheight)
    {
        title->measure(availablewidth, availableheight);
        pick->measure(availablewidth, availableheight);
        teammate1->measure(availablewidth, availableheight);
        teammate2->measure(availablewidth, availableheight);
        teammate3->measure(availablewidth, availableheight);
        teammate4->measure(availablewidth, availableheight);
        flag->measure(availablewidth, availableheight);

        menu->measure(availablewidth, availableheight/4);
        width = availablewidth;
        height = availableheight;
    }

    void render(int x, int y)
    {
        title->render(x + width/2 - text_width(title->text)/2, y + height/8);
        pick->render(x + width/2 - text_width(pick->text)/2, y + 4*height/16);

        flag->render(x, y);
        teammate1->render(x, y);
        teammate2->render(x, y);
        teammate3->render(x, y);
        teammate4->render(x, y);

        int menux = x+(width-menu->width)/2;
        int menuy = y+height-menu->height;
        menu->render(menux, menuy);

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;
    }

    virtual void bubbleevent(uievent e)
    {
        if(e.type == uievent::UIE_SELECTED)
        {
            int key = menu->items[menu->selection]->key;
            team = key&1 ? RVSF : CLA;
            setupmodels();

            uievent teamchangedevent;
            teamchangedevent.emitter = this;
            teamchangedevent.type = uievent::UIE_SELECTED;
            teamchangedevent.data = team;
            view::bubbleevent(teamchangedevent);
            return;
        }
        view::bubbleevent(e);
    }

    void setupmodels()
    {
        setupplayer(teammate1, 0);
        setupplayer(teammate2, 1);
        setupplayer(teammate3, 2);
        setupplayer(teammate4, 3);
        setupflag();
    }

    void setupplayer(modelview *player, int offset)
    {
        formatstring(player->mdl)("playermodels");
        player->translatey = -0.4f;
        player->translatex = offset * 0.18f * (team == CLA ? 1.0f : -1.0f);
        player->animbasetime = offset * 600;
        player->scale = 0.1f;
        player->anim = ANIM_IDLE | ANIM_LOOP;
        player->rotatex = -90;
        player->rotatez = -90;
        player->yaw = (25 + 10*rnd(1000)/1000) * (team == CLA ? 1 : -1);

        extern const char *gunnames[];
        formatstring(weaponmdls[offset])("weapons/%s/world", gunnames[GUN_CARBINE + offset]);
        player->modelattach[0].name = weaponmdls[offset];
        player->modelattach[0].tag = "tag_weapon";

        const char *teamname = NULL;
        const char *picktext = NULL;
        const char *skinname = NULL;
        int skin = offset % 3;
        if(team == CLA)
        {
            teamname = "CLA";
            picktext = "\f4CLA // Cubers Liberation Army";
            skinname = (skin == 0 ? "01" : skin == 1 ? "02" : "05");
        }
        else
        {
            teamname = "RVSF";
            picktext = "\f4RVSF // Rabid Viper Special Forces";
            skinname = (skin == 0 ? "01" : skin == 1 ? "06" : "08");
        }
        pick->settext(picktext);
        defformatstring(mdlskin)("packages/models/playermodels/%s/%s.jpg", teamname, skinname);
        player->tex =  -(int)textureload(mdlskin)->id;
    }

    void setupflag()
    {
        int key = menu->items[menu->selection]->key;
        team = key&1 ? RVSF : CLA;
        formatstring(flag->mdl)("pickups/flags/%s", team == CLA ? "CLA" : "RVSF");
        flag->translatey = -0.4f;
        flag->translatex = 0.65f * (team == CLA ? 1.0f : -1.0f);
        flag->translatez = 0.5f;
        flag->animbasetime = 0;
        flag->scale = 0.1f;
        flag->anim = ANIM_FLAG | ANIM_LOOP;
        flag->rotatex = -90;
        flag->rotatez = -45;
    }
};