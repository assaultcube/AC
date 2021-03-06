// the touch menu item is the base class for all items in a touch menu
struct touchmenuitem : view
{
    int key = -1;
    touchmenuitem(view *parent, int key) : view(parent), key(key) {}
    virtual ~touchmenuitem() {}
    virtual void measure(int availablewidth, int availableheight) = 0;
    virtual void render(int x, int y) = 0;
    virtual void onselected() = 0;
    virtual void ondeselected() = 0;
};

// the image touch menu item a selectable image
struct imagetouchmenuitem : touchmenuitem
{
    struct touchmenu *parent;
    char *texname;
    Texture *tex = NULL;
    int row, col;
    float tsx, tsy;
    int contentsize;
    int padding;
    bool isselected = false;
    bool circleborder = false;

    imagetouchmenuitem(view *parent, int key, const char *texname, float tsx, float tsy, int row, int col)
            : touchmenuitem(parent, key), texname(newstring(texname)), row(row), col(col), tsx(tsx), tsy(tsy) {};

    ~imagetouchmenuitem()
    {
        DELETEP(texname);
    }

    void measure(int availablewidth, int availableheight)
    {
        width = height = min(availablewidth, availableheight);
        padding = width/10;
        contentsize = width-2*padding;
    }

    void render(int x, int y)
    {
        if(!visible) return; // fixme: do this in call structs..

        if(isselected) {
            int anim = (int) ((sinf((float)lastmillis/300.0f)+1.0f)/2.0f * ((float)padding/2.0f));
            blendbox(x + anim, y + anim, x + contentsize + 2*padding - anim, y + contentsize + 2*padding - anim, false, -1, &config.selectedcolor);
        }

        if(circleborder)
        {
            int xcenter = x+padding+contentsize/2;
            int ycenter = y+padding+contentsize/2;
            int outercircleradius = contentsize/2;
            int innnercircleradius = contentsize/2-padding;
            glColor3f(0.0f, 0.0f, 0.0f);
            circle(-1, xcenter, ycenter, outercircleradius, 0, 0, 0, 64);
            glColor3f(1.0f, 1.0f, 1.0f);
            circle(-1, xcenter, ycenter, innnercircleradius, 0, 0, 0, 64);
            glColor3f(0.0f, 0.0f, 0.0f);

            glEnable(GL_BLEND);
            glColor4f(1, 1, 1, 1);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            if(!tex) tex = textureload(texname, 3);
            int safepx = 0;
            if(tex) quad(tex->id, x + padding + padding + safepx, y + padding + padding + safepx, contentsize - 2*padding - 2*safepx, tsx*col, tsy*row, tsx, tsy);
        }
        else
        {
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            if(!tex) tex = textureload(texname, 3);
            if(tex) quad(tex->id, x + padding, y + padding, contentsize, tsx*col, tsy*row, tsx, tsy);
        }

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;
    }

    virtual void onselected() { isselected = true; }
    virtual void ondeselected() { isselected = false; }
};

// the solid menu item is a selectable colored box
struct solidmenuitem : touchmenuitem
{
    struct touchmenu *parent;
    color &c;
    int contentsize;
    int padding;
    bool isselected = false;

    solidmenuitem(view *parent, int key, color &c)
            : touchmenuitem(parent, key), c(c) {};

    ~solidmenuitem()
    {
    }

    void measure(int availablewidth, int availableheight)
    {
        width = height = min(availablewidth, availableheight);
        padding = width/10;
        contentsize = width-2*padding;
    }

    void render(int x, int y)
    {
        if(isselected)
        {
            int anim = (int) ((sinf((float)lastmillis/300.0f)+1.0f)/2.0f * ((float)padding/2.0f));
            blendbox(x + anim, y + anim, x + contentsize + 2*padding - anim, y + contentsize + 2*padding - anim, false, -1, &config.selectedcolor);
        }
        blendbox(x + padding, y + padding, x + padding + contentsize, y + padding + contentsize, false, -1, &c);

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;
    }

    virtual void onselected() { isselected = true; }
    virtual void ondeselected() { isselected = false; }
};

// the touch menu is a set of horizontal stacks nested inside a vertical stack
struct touchmenu : view
{
    vector<touchmenuitem *> items; // determines items of menu which are rendered in this order
    int cols = 2; // determines fixed count of columns
    int rows = 2; // determines count of rows as a hint for height calculation, however rows are dynamic
    int padding;
    int selection = -1;

    touchmenu(view *parent) : view(parent) {}
    virtual ~touchmenu()
    {
        items.deletecontents();
    }

    virtual void measure(int availablewidth, int availableheight)
    {
        int rowwidth = 0, rowheight = 0, col = 0, row = 0;
        width = height = padding = 0;

        int itemsize = min(availablewidth / (2/10.0f + cols), availableheight / (2/10.0f + rows));
        padding = itemsize / 10;

        loopv(items)
        {
            items[i]->measure(itemsize, itemsize);
            rowwidth += items[i]->width;
            rowheight = max(rowheight, items[i]->height);
            col++;
            if(col>=cols || i==items.length()-1)
            {
                col = 0;
                row++;
                width = max(width, rowwidth);
                height += rowheight;
                rowwidth = rowheight = 0;
            }
        }

        width += 2*padding;
        height += 2*padding;
    }

    // render all items given a fixed number of columns and a variable number of rows
    void render(int x, int y)
    {
        int rowwidth = 0, rowheight = 0, col = 0, row = 0, xoffset = 0, yoffset = 0;
        loopv(items)
        {
            items[i]->render(x + padding + xoffset, y + padding + yoffset);
            rowwidth += items[i]->width;
            rowheight = max(rowheight, items[i]->height);
            xoffset = rowwidth;
            col++;
            if(col>=cols || i==items.length()-1)
            {
                col = 0;
                row++;
                yoffset += rowheight;
                rowwidth = xoffset = rowheight = 0;
            }
        }

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;
    }

    virtual void bubbleevent(uievent e)
    {
        view::bubbleevent(e);
        // detect selection of item by user and bubble additional selection event
        if(e.type == uievent::UIE_TAPPED)
        {
            int idx = -1;
            loopv(items) if(items[i] == e.emitter) { idx = i; break; }
            if(idx >= 0)
            {
                select(idx);
                uievent selectionuievent;
                selectionuievent.emitter = e.emitter;
                selectionuievent.type = uievent::UIE_SELECTED;
                if(parent) parent->bubbleevent(selectionuievent);
                audiomgr.playsoundc(S_MENUENTER, player1, SP_HIGHEST);
            }
        }
    }

    // select the item at given index
    virtual void select(int idx)
    {
        if(idx < 0 || idx >= items.length()) return;

        // propagate state change to new item and previous item
        if(!(selection < 0 || selection >= items.length()))
        {
            items[selection]->ondeselected();
        }
        selection = idx;
        items[idx]->onselected();

        // bubble selection event
        uievent selectionuievent;
        selectionuievent.emitter = items[idx];
        selectionuievent.type = uievent::UIE_SELECTED;
        if(parent) parent->bubbleevent(selectionuievent);
    }
};