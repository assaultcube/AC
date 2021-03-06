// the skin scene allows choosing the skin
struct skinscene : view
{
    textview *title;
    modelview *model;
    touchmenu *menu;
    color firstskincolor, secondskincolor, thirdskincolor;
    int weapon = 0;
    string weaponmdl;

    enum skintype : int { FIRST = 0, SECOND, THIRD };
    skintype skin = FIRST;

    enum teamtype : int { CLA = 0, RVSF };
    teamtype team = CLA;

    skinscene(view *parent) : view(parent)
    {
        title = new textview(this, "Preferred look", false);

        model = new modelview(this);
        formatstring(model->mdl)("playermodels");
        model->translatey = -0.5f;
        model->scale = 0.2f;
        model->anim = ANIM_IDLE | ANIM_LOOP;
        children.add(model);

        menu = new touchmenu(this);
        menu->rows = 1, menu->cols = 3;
        menu->items.add(new solidmenuitem(menu, (int) FIRST, firstskincolor));
        menu->items.add(new solidmenuitem(menu, (int) SECOND, secondskincolor));
        menu->items.add(new solidmenuitem(menu, (int) THIRD, thirdskincolor));

        loopv(menu->items) menu->children.add(menu->items[i]);
        children.add(menu);
    };

    ~skinscene()
    {
        DELETEP(title);
        DELETEP(model);
        DELETEP(menu);
    }

    virtual void oncreate() {
        view::oncreate();
        menu->select(0);
    };

    virtual void measure(int availablewidth, int availableheight)
    {
        title->measure(availablewidth, availableheight);
        model->measure(availablewidth, availableheight);
        menu->measure(availablewidth, availableheight/4);
        width = availablewidth;
        height = availableheight;
    }

    void render(int x, int y)
    {
        title->render(x + width/2 - text_width(title->text)/2, y + height/8);
        model->render(x, y);

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
            updatemodel();
        }
        view::bubbleevent(e);
    }

    void updatemodel()
    {
        if (team == CLA)
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

        int key = menu->items[menu->selection]->key;
        skin = key == 0 ? FIRST : key == 1 ? SECOND : THIRD;

        formatstring(model->mdl)("playermodels");
        model->translatey = -0.5f;
        model->translatex = 0.0f;
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
        if(team == CLA)
        {
            teamname = "CLA";
            skinname = (skin == FIRST ? "01" : skin == SECOND ? "02" : "05");
        }
        else
        {
            teamname = "RVSF";
            skinname = (skin == FIRST ? "01" : skin == SECOND ? "06" : "08");
        }
        defformatstring(mdlskin)("packages/models/playermodels/%s/%s.jpg", teamname, skinname);
        model->tex =  -(int)textureload(mdlskin)->id;
    }
};