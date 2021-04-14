// the weapon scene allows choosing the primary weapon
struct weaponscene : view
{
    textview *title;
    textview *pick;
    modelview *weapon;
    touchmenu *menu;

    weaponscene(view *parent) : view(parent)
    {
        title = new textview(this, "Preferred weapon", false);
        pick = new textview(this, "", false );

        weapon = new modelview(this);
        weapon->translatey = 0.9f;
        weapon->scale = 0.5f;
        weapon->anim = 0;
        weapon->rotatex = -90;
        weapon->rotatez = -90;
        children.add(weapon);

        menu = new touchmenu(this);
        menu->rows = 1, menu->cols = 5;
        menu->items.add(new imagetouchmenuitem(menu, GUN_CARBINE, "packages/misc/items.png", 1.0/4.0f,1.0/4.0f, 2, 0));
        menu->items.add(new imagetouchmenuitem(menu, GUN_SHOTGUN, "packages/misc/items.png", 1.0/4.0f, 1.0/4.0f, 3, 0));
        menu->items.add(new imagetouchmenuitem(menu, GUN_SUBGUN, "packages/misc/items.png", 1.0/4.0f, 1.0/4.0f, 0, 1));
        menu->items.add(new imagetouchmenuitem(menu, GUN_SNIPER, "packages/misc/items.png", 1.0/4.0f, 1.0/4.0f, 1, 1));
        menu->items.add(new imagetouchmenuitem(menu, GUN_ASSAULT, "packages/misc/items.png", 1.0/4.0f, 1.0/4.0f, 2, 1));
        menu->children.add(menu->items[0]);
        menu->children.add(menu->items[1]);
        menu->children.add(menu->items[2]);
        menu->children.add(menu->items[3]);
        menu->children.add(menu->items[4]);
        children.add(menu);
    };

    ~weaponscene()
    {
        DELETEP(title);
        DELETEP(pick);
        DELETEP(weapon);
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
        weapon->measure(availablewidth, availableheight);
        menu->measure(availablewidth, availableheight/4);
        width = availablewidth;
        height = availableheight;
    }

    void render(int x, int y)
    {
        title->render(x + width/2 - text_width(title->text)/2, y + height/8);
        pick->render(x + width/2 - text_width(pick->text)/2, y + 3*height/16);

        weapon->yaw = lastmillis / 20.0f;
        weapon->render(x, y);
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
            loopv(menu->items) if(e.emitter == menu->items[i])
            {
                int newweapon = menu->items[i]->key;
                selectweapon(newweapon);

                uievent weapevent;
                weapevent.type = uievent::UIE_SELECTED;
                weapevent.emitter = this;
                weapevent.data = newweapon;
                view::bubbleevent(weapevent);
                return;
            }
        }
        view::bubbleevent(e);
    }

    void selectweapon(int weap)
    {
        formatstring(weapon->mdl)("weapons/%s/menu", gunnames[weap]);
        defformatstring(curweapdesc)( "%s: %s", gunnames[weap], weapstr(weap));
        pick->settext(curweapdesc);//weapstr(weap));//gunnames[weap]
    }
};