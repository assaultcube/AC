// the server menu item is a selectable box that shows basic server infos
struct servermenuitem : touchmenuitem
{
    struct touchmenu *parent;
    int contentsize;
    int padding;
    bool isselected = false;

    enum battlegroundtype {
        BG_TRAINING = 0,
        BG_PLAYOFFLINE,
        BG_PLAYONLINE
    } battlegroundtype;

    const char *map;
    Texture *image = NULL;
    int numplayers = 0, maxplayers = 0;
    string text;

    const int taptolerance = VIRTH/50;
    int fingerdown = 0;
    float dx = 0, dy = 0;

    servermenuitem(view *parent, int key, const char *map)
            : touchmenuitem(parent, key), map(map)
    {
        // move to oncreate?
        extern int hidebigmenuimages; // fixme
        if(!image)
        {
            silent_texture_load = true;
            const char *cgzpath = "packages" PATHDIVS "maps" PATHDIVS "official";
            defformatstring(p2p)("%s/preview/%s.jpg", cgzpath, map);
            if(!hidebigmenuimages) image = textureload(p2p, 3);
            if(!image || image == notexture){
                image = textureload("packages/misc/nopreview.jpg", 3);
            }
            silent_texture_load = false;
        }
    };

    ~servermenuitem()
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
        bool serverloading = (maxplayers == 0);
        bool serverfull = numplayers >= maxplayers;
        bool serverempty = !numplayers;

        if(isselected) {
            int anim = (int) ((sinf((float)lastmillis/300.0f)+1.0f)/2.0f * ((float)padding/2.0f));
            blendbox(x + anim, y + anim, x + contentsize + 2*padding - anim, y + contentsize + 2*padding - anim, false, -1, &config.selectedcolor);
        }

        extern int hidebigmenuimages; // fixme
        if(image && !hidebigmenuimages)
        {
            if(serverfull && battlegroundtype == BG_PLAYONLINE)
            {
                // draw the quad greyed-out
                glDisable(GL_BLEND);
                color gray(0.4f, 0.4f, 0.4f);
                blendbox(x + padding, y + padding, x + padding + contentsize, y + padding + contentsize, false, -1, &gray);
                glEnable(GL_BLEND);
                glColor4f(1.0f, 1.0f, 1.0f, 0.2f);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            }
            else
            {
                // draw the quad normally
                glDisable(GL_BLEND);
                glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            }

            quad(image->id, x + padding, y + padding, contentsize, 0, 0, 1.0f, 1.0f);

            glEnable(GL_BLEND);

            string text1;
            switch(battlegroundtype)
            {
                case BG_TRAINING:
                    formatstring(text1)("Solo/Training");
                    draw_text(text1, x + padding*3/2, y + padding*3/2);
                    break;
                case BG_PLAYOFFLINE:
                    formatstring(text1)("Play offline");
                    draw_text(text1, x + padding*3/2, y + padding*3/2);
                    draw_text(text, x + width/2 - text_width(text)/2, y + height/2 - FONTH/2);
                    break;
                case BG_PLAYONLINE:
                    formatstring(text1)("Play online");
                    draw_text(text1, x + padding*3/2, y + padding*3/2);
                    if(serverloading) formatstring(text)("LOADING");
                    else formatstring(text)("%s%i/%i", serverfull ? "FULL " : serverempty ? "EMPTY " : "", numplayers, maxplayers);
                    draw_text(text, x + contentsize - text_width(text) + padding/2, y + contentsize - FONTH + padding/2);
                    break;
            }
        }

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;
    }

    virtual void onselected() { isselected = true; }
    virtual void ondeselected() { isselected = false; }

    virtual void captureevent(SDL_Event *event)
    {
        if(event->type == SDL_FINGERDOWN)
        {
            fingerdown = lastmillis;
        }
        else if(event->type == SDL_FINGERMOTION)
        {
            if(fingerdown)
            {
                dx += abs(event->tfinger.dx);
                dy += abs(event->tfinger.dy);
            }
        }
        if(event->type == SDL_FINGERUP)
        {
            if(fingerdown)
            {
                if(abs(dx * VIRTW) < taptolerance && abs(dy * VIRTH) < taptolerance)
                {
                    uievent uievent;
                    uievent.emitter = this;
                    uievent.type = uievent.UIE_TAPPED;
                    bubbleevent(uievent);
                }
                fingerdown = 0;
                dx = dy = 0;
            }
        }
    }
};

// the server scene allows choosing the server to connect to
struct serverscene : view
{
    const int refreshrate = 2000; // determines the interval in millisesconds for refreshing the list
    const int maxserverrows = 64; // determines the max number of servers
    const int creationmillis;

    textview *title = NULL, *mastermessage = NULL;
    touchmenu *menu;
    color c = color(0.11f, 0.11f, 0.11f);
    const int playofflineoffset = 1;
    const int playonlineoffset = 4;
    int lastrefresh = 0;
    vector<serverinfo> lastservers;
    serverinfo selectedserver;

    int menuoffsetx = 0;
    int menuoffsety = 0;

    // scrolling
    bool scrolling = false;
    int yscroll = 0;
    int lastscrollmillis = 0;
    float lastscrollvelocity = 0;

    serverscene(view *parent) : view(parent), creationmillis(lastmillis)
    {
        title = new textview(this, "Choose your battleground", false);
        children.add(title);

        menu = new touchmenu(this);
        menu->rows = 1, menu->cols = 4;
        loopv(menu->items) menu->children.add(menu->items[i]);
        children.add(menu);

        extern string mastermessagestr;
        mastermessage = new textview(this, mastermessagestr, false);
        children.add(mastermessage);
    };

    ~serverscene()
    {
        DELETEP(title);
        DELETEP(mastermessage);
        DELETEP(menu);
    }

    virtual void oncreate() {
        view::oncreate();

        if(config.UPDATEFROMMASTER)
        {
            int force = 1;
            updatefrommaster(&force);
        }

        refreshservers(NULL, true);
        updateservers();

        menu->select(0);
    };

    void updateservers()
    {
        extern vector<serverinfo *> servers;
        bool hasselectedaddress = menu->selection >= playonlineoffset;

        int fingerdown = -1, fingerdownmillis = 0;
        loopv(menu->items)
        {
            if(((servermenuitem *) menu->items[i])->fingerdown > fingerdownmillis)
            {
                fingerdownmillis = ((servermenuitem *) menu->items[i])->fingerdown;
                fingerdown = i;
            }
        }

        loopirev(menu->items.length())
        {
            DELETEP(menu->items[i]);
            menu->items.remove(i);
        }
        loopirev(menu->children.length()) menu->children.remove(i);

        menu->rows = min(maxserverrows, (servers.length() + playonlineoffset) / menu->cols);

        // training
        servermenuitem *mitem = new servermenuitem(menu, 0, config.TRAINING_MAP);
        copystring(mitem->text, "");
        mitem->numplayers = 0;
        mitem->maxplayers = 0;
        mitem->battlegroundtype = servermenuitem::BG_TRAINING;
        mitem->isselected = (menu->selection == 0);
        menu->items.add(mitem);
        menu->children.add(mitem);

        // play offline
        loopi(config.PLAYOFFLINE_NUM_DIFFICULTIES)
        {
            // randomly pick a map per difficulty level based on time
            int hash = abs(creationmillis + i);
            const char *map = config.SUPPORTEDMAPS[hash % config.NUM_SUPPORTEDMAPS];
            int key = playofflineoffset + i; // fixme
            servermenuitem *mitem = new servermenuitem(menu, key, map);
            copystring(mitem->text, config.PLAYOFFLINE_DIFFICULTIES[i]);
            mitem->numplayers = 0;
            mitem->maxplayers = 0;
            mitem->battlegroundtype = servermenuitem::BG_PLAYOFFLINE;
            mitem->isselected = (menu->selection == playofflineoffset + i);
            menu->items.add(mitem);
            menu->children.add(mitem);
        }

        // play online
        int addedservers = 0;
        loopv(servers)
        {
            if(i > menu->rows * menu->cols) break; // fixme

            // You don't want to show the user 30+ loading servers when only a subset of them will eventually be alive
            // and so we simply limit this amount to a reasonable number.
            bool isloading = (servers[i]->maxclients == 0);
            if(isloading && addedservers >= config.PLAYONLINE_MINSERVERS) continue;

            bool issupportedmap = false;
            loopj(config.NUM_SUPPORTEDMAPS)
            {
                if(strcmp(config.SUPPORTEDMAPS[j], servers[i]->map) == 0 || !servers[i]->numplayers) issupportedmap = true;
            }
            if(!issupportedmap) continue;

            // if the server is empty then there is no current map
            // so we simply pick a random map based on current time the hashed server address
            int hash = abs(creationmillis + (int) servers[i]->address.host ^ servers[i]->address.port);
            const char *newmap = config.SUPPORTEDMAPS[hash % config.NUM_SUPPORTEDMAPS];

            const char *map = !servers[i]->numplayers ? newmap : servers[i]->map;
            servermenuitem *mitem = new servermenuitem(menu, 0, map);
            mitem->numplayers = servers[i]->numplayers;
            mitem->maxplayers = servers[i]->maxclients;
            mitem->battlegroundtype = servermenuitem::BG_PLAYONLINE;

            if(selectedserver.address.host == servers[i]->address.host && selectedserver.address.port == servers[i]->address.port)
                mitem->isselected = true;

            menu->items.add(mitem);
            menu->children.add(mitem);
            addedservers++;
        }

        // restore position of the selected item so that it does not move around on the screen when we recreate the list
        if(hasselectedaddress)
        {
            loopv(menu->items)
            {
                servermenuitem *mitem = (servermenuitem *) menu->items[i];
                if(mitem->isselected) {
                    menu->items.remove(i);
                    menu->items.insert(menu->selection, mitem);
                    break;
                }
            }
        }

        // restore fingerdown state which was lost during item creation
        if(fingerdown >= 0) ((servermenuitem *)menu->items[fingerdown])->fingerdown = true;

        lastservers.setsize(0);
        loopv(servers) lastservers.add(*servers[i]);
    }

    virtual void measure(int availablewidth, int availableheight)
    {
        title->measure(availablewidth, availableheight);
        menu->measure(availablewidth, availableheight*1000); // fixme
        mastermessage->measure(availablewidth, availableheight);
        width = availablewidth;
        height = availableheight;
    }

    void render(int x, int y)
    {
        title->render(x + width/2 - text_width(title->text)/2, y + height/8);

        menuoffsetx = x+(width-menu->width)/2;
        menuoffsety = y+VIRTH/3; // better place for this?
        menu->render(menuoffsetx, menuoffsety + (int)yscroll);

        mastermessage->render(x + width/2 - text_width(mastermessage->text)/2, title->bbox.y2 + (menu->bbox.y1 - title->bbox.y2)/2);

        bbox.x1 = x;
        bbox.x2 = x + width;
        bbox.y1 = y;
        bbox.y2 = y + height;

        if(lastmillis - lastrefresh >= refreshrate)
        {
            refreshservers(NULL, false);
            updateservers();
            lastrefresh = lastmillis;
        }

        if(!scrolling && lastscrollmillis > 0)
        {
            int millis = lastmillis - lastscrollmillis;

            float throttling = millis/10.0f * (lastscrollvelocity > 0.0f ? -1.0f : 1.0f);
            float fadeoutscroll = lastscrollvelocity * VIRTH + throttling;
            if((lastscrollvelocity > 0.0f && fadeoutscroll > 0.0f) || (lastscrollvelocity < 0.0f && fadeoutscroll < 0.0f))
                yscroll += fadeoutscroll;
            else
                lastscrollmillis = 0;

            limitscrolling();
        }
    }

    virtual void captureevent(SDL_Event *event)
    {
        view::captureevent(event);
        switch (event->type) {
            case SDL_FINGERDOWN:
                scrolling = true;
                lastscrollmillis = 0;
                break;
            case SDL_FINGERUP:
                scrolling = false;
                lastscrollmillis = lastmillis;
                break;
            case SDL_FINGERMOTION:
                lastscrollvelocity = event->tfinger.dy;
                yscroll += event->tfinger.dy * VIRTH;
                limitscrolling();
                break;
        }
    }

    virtual void bubbleevent(uievent event) {
        if(event.type == uievent::UIE_SELECTED)
        {
            if(menu->selection >= playonlineoffset) selectedserver = lastservers[menu->selection - playonlineoffset];
            else memset(&selectedserver, 0, sizeof(serverinfo));
        }
        view::bubbleevent(event);
    };

    void limitscrolling()
    {
        yscroll = min(0, yscroll);
        int scrollviewportheight = (height - menuoffsety);
        yscroll = max(-menu->height+scrollviewportheight, yscroll);
    }
};