// the equipmentscene scene allows to configure weapon, team and skin while staying in the game
struct equipmentscene : view
{
    textview *title;
    navigationbutton *prevbutton;
    modelview *model;
    touchmenu *teammenu;
    touchmenu *skinmenu;
    touchmenu *weaponmenu;
    color firstskincolor, secondskincolor, thirdskincolor;

    int weapon = 0;
    string weaponmdl;
    game::settings::teamtype team;
    game::settings::skintype skin;

    equipmentscene(view *parent) : view(parent)
    {
        title = new textview(this, "Gear", false);

        prevbutton = new navigationbutton(this, navigationbutton::PREV);
        children.add(prevbutton);

        model = new modelview(this);
        formatstring(model->mdl)("playermodels");
        children.add(model);

        teammenu = new touchmenu(this);
        teammenu->rows = 1, teammenu->cols = 2;
        teammenu->items.add(new imagetouchmenuitem(teammenu, game::settings::RVSF, "packages/misc/teams.png", 1.0 / 2.0f, 1.0f, 0, 1));
        teammenu->items.add(new imagetouchmenuitem(teammenu, game::settings::CLA, "packages/misc/teams.png", 1.0 / 2.0f, 1.0f, 0, 0));
        loopv(teammenu->items) teammenu->children.add(teammenu->items[i]);
        children.add(teammenu);

        skinmenu = new touchmenu(this);
        skinmenu->rows = 1, skinmenu->cols = 3;
        skinmenu->items.add(new solidmenuitem(skinmenu, (int) game::settings::FIRST, firstskincolor));
        skinmenu->items.add(new solidmenuitem(skinmenu, (int) game::settings::SECOND, secondskincolor));
        skinmenu->items.add(new solidmenuitem(skinmenu, (int) game::settings::THIRD, thirdskincolor));
        loopv(skinmenu->items) skinmenu->children.add(skinmenu->items[i]);
        children.add(skinmenu);

        weaponmenu = new touchmenu(this);
        weaponmenu->rows = 1, weaponmenu->cols = 5;
        weaponmenu->items.add(new imagetouchmenuitem(weaponmenu, GUN_CARBINE, "packages/misc/items.png", 1.0/4.0f,1.0/4.0f, 0, 2));
        weaponmenu->items.add(new imagetouchmenuitem(weaponmenu, GUN_SHOTGUN, "packages/misc/items.png", 1.0/4.0f, 1.0/4.0f, 0, 3));
        weaponmenu->items.add(new imagetouchmenuitem(weaponmenu, GUN_ASSAULT, "packages/misc/items.png", 1.0/4.0f, 1.0/4.0f,1, 0));
        weaponmenu->items.add(new imagetouchmenuitem(weaponmenu, GUN_SNIPER, "packages/misc/items.png", 1.0/4.0f, 1.0/4.0f, 1, 1));
        weaponmenu->items.add(new imagetouchmenuitem(weaponmenu, GUN_SUBGUN, "packages/misc/items.png", 1.0/4.0f, 1.0/4.0f, 1, 2));
        loopv(weaponmenu->items) weaponmenu->children.add(weaponmenu->items[i]);
        children.add(weaponmenu);
    };

    ~equipmentscene()
    {
        DELETEP(title);
        DELETEP(prevbutton);
        DELETEP(model);
        DELETEP(teammenu);
        DELETEP(skinmenu);
        DELETEP(weaponmenu);
    }

    virtual void oncreate()
    {
        view::oncreate();

        game.settings.load();

        loopv(skinmenu->items)
        if(skinmenu->items[i]->key == game.settings.skin) { skinmenu->select(i); break; }

        loopv(weaponmenu->items)
        if(weaponmenu->items[i]->key == game.settings.weapon) { weaponmenu->select(i); break; }

        loopv(teammenu->items)
        if(teammenu->items[i]->key == game.settings.team) { teammenu->select(i); break; }
    };

    virtual void measure(int availablewidth, int availableheight)
    {
        title->measure(availablewidth, availableheight);
        prevbutton->measure(availablewidth/4, availableheight/4);
        model->measure(availablewidth, availableheight);
        teammenu->measure(availablewidth, availableheight/6);
        skinmenu->measure(availablewidth, availableheight/6);
        weaponmenu->measure(availablewidth, availableheight/6);
        width = availablewidth;
        height = availableheight;
    }

    void render(int x, int y)
    {
        title->render(x + width/2 - text_width(title->text)/2, y + height/8);

        int itemsize = height/4;
        prevbutton->render(itemsize/2/5, itemsize/2/5);

        model->render(x, y);

        int menusheight = teammenu->height + skinmenu->height + weaponmenu->height;
        int menux = x+width/2-(title->width)/2 - title->padding/2;
        int menuy = y+height/2-menusheight/2;
        teammenu->render(menux, menuy);
        skinmenu->render(menux, menuy + teammenu->height);
        weaponmenu->render(menux, menuy + teammenu->height + skinmenu->height);

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
                    game.settings.team = team;
                    game.settings.weapon = weapon;
                    game.settings.skin = skin;
                    game.settings.save();
                    viewstack.deletecontents();
                    return;
                }
                break;
            case uievent::UIE_SELECTED:
                if(e.emitter->parent == weaponmenu) // fixme: should be raised by parent instead?
                {
                    weapon = weaponmenu->items[weaponmenu->selection]->key;
                    updatemodel();
                }
                else if(e.emitter->parent == teammenu)
                {
                    team = (game::settings::teamtype) teammenu->items[teammenu->selection]->key;
                    updatemodel();
                }
                else if(e.emitter->parent == skinmenu)
                {
                    skin = (game::settings::skintype) e.data;
                    updatemodel();
                }
                break;
            default:
                break;
        }
        view::bubbleevent(e);
    }

    void updatemodel()
    {
        if(team == game::settings::CLA)
        {
            firstskincolor = color(0.11f, 0.11f, 0.11f);
            secondskincolor = color(0.16f, 0.17f, 0.11f);
            thirdskincolor = color(0.38f, 0.33f, 0.18f);
        }
        else
        {
            firstskincolor = color(0.06f, 0.12f, 0.15f);
            secondskincolor = color(0.18f, 0.18f, 0.18f);
            thirdskincolor = color(0.30f, 0.36f, 0.24f);
        }

        int key = skinmenu->items[skinmenu->selection]->key;
        skin = key == 0 ? game::settings::FIRST : key == 1 ? game::settings::SECOND : game::settings::THIRD;

        formatstring(model->mdl)("playermodels");
        model->translatey = -0.5f;
        model->translatex = -0.5f;
        model->animbasetime = 0;
        model->scale = 0.2f;
        model->anim = ANIM_IDLE | ANIM_LOOP;
        model->rotatex = -90;
        model->rotatez = -90;
        model->yaw = -(25 + 10*rnd(1000)/1000);

        formatstring(weaponmdl)("weapons/%s/world", gunnames[weapon]);
        model->modelattach[0].name = weaponmdl;
        model->modelattach[0].tag = "tag_weapon";

        const char *teamname = NULL;
        const char *skinname = NULL;
        if(team == game::settings::CLA)
        {
            teamname = "CLA";
            skinname = (skin == game::settings::FIRST ? "01" : skin == game::settings::SECOND ? "02" : "05");
        }
        else
        {
            teamname = "RVSF";
            skinname = (skin == game::settings::FIRST ? "01" : skin == game::settings::SECOND ? "06" : "08");
        }
        defformatstring(mdlskin)("packages/models/playermodels/%s/%s.jpg", teamname, skinname);
        model->tex =  -(int)textureload(mdlskin)->id;
    }
};