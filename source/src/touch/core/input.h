// handles the user input and manages virtual keys on the touch display
struct input
{
    // represents a press, release or move event related to a virtual key that is located on the touch display
    struct keyevent
    {
        int type = 0;
        int key = 0;

        keyevent(int type, int key) : type(type), key(key)
        {}
    };

    const int iconsize = config.HUD_ICONSIZE;
    vector <SDL_Event> events; // events to process
    hashtable<int, vector<keyevent*>*> touchhistory; // remember last pressed and released touch keys indexed by finger

    int lookingdirectionfinger = -1; // determines the id of the finger that controls the looking direction of the player
    int movementdirectionfinger = -1; // determines the id of the finger that controls the movement direction of the player
    int firefinger = -1; // // determines the id of the finger that is controls attacking/firing

    bool movdirdoubletapandhold = false; // determines if the user is currently doing a 'double tap and hold' with the movement direction finger

    int lastmovementdirectiontap = -1, lastlookingdirectiontap = -1;
    hashtable<int, vector<int>*> lastpressedtouchkeys; // remember last pressed touch key indexed by finger
    vec *touch1 = new vec(0, 0, 0);

    // determine which virtual keys on the mobile screen were pressed or released
    // unlike a hardware keyboard the virtual keys are dynamic depending on context
    vector <keyevent> gettouchkeys(SDL_Event &event)
    {
        vector <keyevent> keys;

        if(player1->state == CS_DEAD || player1->state == CS_SPECTATE)
        {
            deadspectatekeys(event, keys);
        }
        else
        {
            identifyfingers(event);
            alivekeys(event, keys);
        }

        return keys;
    }

    // test for tapping inside an area surrounding the boundingbox of the icon. (x0,y0)-(x0+w,y0+h)
    bool icontapped(int vfingerx, int vfingery, int x0, int y0 )
    {
        int air = iconsize / 2;
        int ta_x0 = x0 - air;
        int ta_x1 = x0 + iconsize + air;
        int ta_y0 = y0 - air;
        int ta_y1 = y0 + iconsize + air;
        return ( ( vfingerx >= ta_x0 && vfingerx <= ta_x1 ) && ( vfingery >= ta_y0 && vfingery <= ta_y1 ) );
    }

    // determines keys in dead or spectate mode
    void deadspectatekeys(SDL_Event &event, vector <keyevent> &keys)
    {
        /*
         * the default action should be to respawn - by calling physics.cpp:attack()
         * so first check the "between respawns" opportunities given:
         * settings, equipment left-corners top/bottom
         * and voicecoms - this part of the GUI is WIP ATM
         */
        bool deaddeed = false; // did we do something while dead or just tapped to respawn?
        // TODO: think about the redundancy between hud.h:draw() and input.h:deadspectatekeys()
        int vfingerx = event.tfinger.x * VIRTW;
        int vfingery = event.tfinger.y * VIRTH;
        int iconsizetwice = iconsize * 2; // a bit of air around the corner icons
        int touchEdgeRight = VIRTW - iconsizetwice; // strip in which icons on the right edge reside
        int touchEdgeBottom = VIRTH - iconsizetwice; // strip in which icons on the bottom edge reside
        if(vfingerx < iconsizetwice && vfingery < iconsizetwice)
        {
            deaddeed = true;
            keys.add(keyevent(event.type, TOUCH_GAME_CORNER_TOP_LEFT)); // open settings scene
        }
        if(vfingerx < iconsizetwice && vfingery > VIRTH-iconsizetwice)
        {
            deaddeed = true;
            keys.add(keyevent(event.type, TOUCH_GAME_CORNER_BOTTOM_LEFT)); // open equipment scene
        }
        if(vfingerx > VIRTW - iconsizetwice && vfingery > VIRTH-iconsizetwice)
        {
            deaddeed = true;
            keys.add(keyevent(event.type, TOUCH_GAME_CORNER_BOTTOM_RIGHT)); // toggle voicecom touchui
        }
        extern int touchoptionstogglestate;
        if(touchoptionstogglestate == 2 ){ // only if the voicecom touchui is ready for tapping
            // icontapped wants our touch-coord and the top-left corner of each icon
            // public voicecom : right edge
            if(icontapped( vfingerx, vfingery, touchEdgeRight, touchEdgeBottom - iconsizetwice))
            {
                deaddeed = true;
                keys.add(keyevent(event.type, TOUCH_GAME_VOICECOM_PUBLIC_1 ));
            }
            if(icontapped( vfingerx, vfingery, touchEdgeRight, touchEdgeBottom - 2 * iconsizetwice))
            {
                deaddeed = true;
                keys.add(keyevent(event.type, TOUCH_GAME_VOICECOM_PUBLIC_2 ));
            }
            // team voicecom : bottom edge
            if(icontapped( vfingerx, vfingery, touchEdgeRight - iconsizetwice, touchEdgeBottom))
            {
                deaddeed = true;
                keys.add(keyevent(event.type, TOUCH_GAME_VOICECOM_TEAM_1));
            }
            if(icontapped( vfingerx, vfingery, touchEdgeRight - 2 * iconsizetwice, touchEdgeBottom))
            {
                deaddeed = true;
                keys.add(keyevent(event.type, TOUCH_GAME_VOICECOM_TEAM_2));
            }
        }

        // no dead deed done?
        if(!deaddeed) keys.add(keyevent(event.type, TOUCH_GAME_RIGHTSIDE_DOUBLETAP));
    }

    // determines keys in alive and non-spectate mode
    void alivekeys(SDL_Event &event, vector <keyevent> &keys)
    {
        bool allowjump = !touchmenuvisible();

        static vec movementcontrolcenter = config.movementcontrolcenter();
        vec finger(event.tfinger.x * VIRTW, event.tfinger.y * VIRTH, 0.0f);
        float dist = movementcontrolcenter.dist(finger);
        int vfingerx = event.tfinger.x * VIRTW;
        int vfingery = event.tfinger.y * VIRTH;
        int icon1x2 = VIRTW*4/8 + iconsize / 2;
        int icon2x1 = VIRTW*5/8 - iconsize / 2;
        int icondiff = (icon2x1 - icon1x2) / 2;

        if(vfingerx >= VIRTW*4/8 - iconsize / 2 - icondiff)
        {
            /*
             * WIP touchkey "suicide" - for quick access to scoreboard & "between respawns" opportunities
             * was CORNER BOTTOM_RIGHT now TOP_RIGHT
             * FYI: 2*iconsize == 3 * iconsize/2 + air
             */
            if(vfingery < 2*iconsize) // top edge of screen
            {
                int touchkey = -1;

                if(vfingerx < VIRTW *5/8 + iconsize / 2 + icondiff) touchkey = TOUCH_GAME_RIGHTSIDE_TOP_0;
                else if(vfingerx < VIRTW*6/8 + iconsize / 2 + icondiff) touchkey = TOUCH_GAME_RIGHTSIDE_TOP_1;
                else if(vfingerx < VIRTW*7/8 + iconsize / 2 + icondiff) touchkey = TOUCH_GAME_RIGHTSIDE_TOP_2;

                // todo: we might need an icon to show the scoreboard on demand, but currently we have too many buttons on the screen already
                else if(vfingerx > VIRTW - 2*iconsize) touchkey = TOUCH_GAME_CORNER_TOP_RIGHT;

                if(touchkey >= 0)
                {
                    keys.add(keyevent(event.type, touchkey));
                    allowjump = false;
                }
            }
        }
        else if(event.tfinger.x < 0.5)
        {
            if(event.tfinger.type == SDL_FINGERDOWN && vfingery < iconsize * 2 &&
               vfingerx < iconsize * 2)
            {
                keys.add(keyevent(event.type, TOUCH_GAME_CORNER_TOP_LEFT));
                allowjump = false;
            }
            else if(event.type == SDL_FINGERDOWN || event.type == SDL_FINGERMOTION)
            {
                vec zero(movementcontrolcenter.x + movementcontrolradius, movementcontrolcenter.y, 0.0f);
                float angle = anglebetween(movementcontrolcenter, zero, finger);
                if(angle < 0.0f) angle = (2 * PI + angle);
                angle = 360.0f - (angle * 360.0f / PI2);

                int touchkey = -1;
                switch(config.MOVEMENTDIRECTIONS)
                {
                    // variant with four sectors which does not support strafe running but is easier to control
                    case config::FOUR_DIRECTIONS:
                    {
                        int sector = ((angle + 45.0f) / 90.0f);
                        switch(sector)
                        {
                            case 0:
                            case 4:
                                touchkey = TOUCH_GAME_LEFTSIDE_RIGHT;
                                break;
                            case 1:
                                touchkey = TOUCH_GAME_LEFTSIDE_TOP;
                                break;
                            case 2:
                                touchkey = TOUCH_GAME_LEFTSIDE_LEFT;
                                break;
                            case 3:
                                touchkey = TOUCH_GAME_LEFTSIDE_BOTTOM;
                                break;
                        }
                        break;
                    }
                    // variant with eight sectors which allows for strafe running but is difficult to master
                    case config::EIGHT_DIRECTIONS:
                    {
                        const int startsectorkey = TOUCH_GAME_LEFTSIDE_TOPRIGHT;
                        const int endsectorkey = TOUCH_GAME_LEFTSIDE_RIGHT;
                        const int numsectors = endsectorkey - startsectorkey + 1;
                        int sector = ((angle + (45.0f / 2.0f)) / 45.0f);
                        touchkey = TOUCH_GAME_LEFTSIDE_TOPRIGHT + sector - 1;
                        if(touchkey < TOUCH_GAME_LEFTSIDE_TOPRIGHT)
                            touchkey = TOUCH_GAME_LEFTSIDE_TOPRIGHT + numsectors - 1;
                        break;
                    }
                }
                if(touchkey >= 0) keys.add(keyevent(event.type, touchkey));
            }
        }

        if(allowjump && event.tfinger.fingerId == movementdirectionfinger && dist > movementcontrolradius)
        {
            keys.add(keyevent(event.type, TOUCH_GAME_LEFTSIDE_OUTERCIRCLE));
        }

        // attack is either bound to VOLUME-UP key or DOUBLET TAP of fire finger
        if(event.tfinger.fingerId == lookingdirectionfinger && event.type == SDL_FINGERDOWN && !game.settings.volumeup)
        {
            if(lastlookingdirectiontap >= 0 && lastmillis - lastlookingdirectiontap < config.DOUBLE_TAP_MILLIS && keys.empty())
            {
                keys.add(keyevent(event.type, TOUCH_GAME_RIGHTSIDE_DOUBLETAP));
            }
            lastlookingdirectiontap = lastmillis;
        }

        // double tap of movement finger will be bound to crouch
        if(event.tfinger.fingerId == movementdirectionfinger && event.type == SDL_FINGERDOWN)
        {
            if(lastmovementdirectiontap >= 0 && lastmillis - lastmovementdirectiontap < config.DOUBLE_TAP_MILLIS) movdirdoubletapandhold = true;
            lastmovementdirectiontap = lastmillis;
        }

        if(event.tfinger.fingerId == movementdirectionfinger && movdirdoubletapandhold)
        {
            // cancel movement when crouching and thumb is located at center of control so that we can crouch without moving
            static vec movementcontrolcenter = config.movementcontrolcenter();
            //already declared up top:vec finger(event.tfinger.x * VIRTW, event.tfinger.y * VIRTH, 0.0f);
            float dist = movementcontrolcenter.dist(finger);
            if(dist < movementcontrolradius / 3) keys.setsize(0);

            keys.add(keyevent(event.type, TOUCH_GAME_LEFTSIDE_DOUBLETAP));
        }
    }

    // identify which fingers are in action
    void identifyfingers(const SDL_Event &event)
    {
        if(event.tfinger.x < 0.5f && movementdirectionfinger == -1 && event.type == SDL_FINGERDOWN)
            movementdirectionfinger = event.tfinger.fingerId;
        else if(event.tfinger.fingerId == movementdirectionfinger && event.type == SDL_FINGERUP)
        {
            movementdirectionfinger = -1;
            movdirdoubletapandhold = false;
        }
        else if(event.tfinger.x * VIRTW >= VIRTW / 2 + iconsize &&
                event.tfinger.x * VIRTW <= VIRTW / 2 + 4 * iconsize
                && event.tfinger.y * VIRTH >= VIRTH / 2 - 2 * iconsize &&
                event.tfinger.y * VIRTH <= VIRTH / 2 + 2 * iconsize
                && firefinger == -1 && event.type == SDL_FINGERDOWN)
            firefinger = event.tfinger.fingerId;
        else if(event.tfinger.fingerId == firefinger && event.type == SDL_FINGERUP)
            firefinger = -1;
        else if(lookingdirectionfinger == -1 && event.type == SDL_FINGERDOWN)
            lookingdirectionfinger = event.tfinger.fingerId;
        else if(event.tfinger.fingerId == lookingdirectionfinger &&
                event.type == SDL_FINGERUP)
            lookingdirectionfinger = -1;
    }

    // process events from SDL to identify finger movement and gestures
    void checkinput()
    {
        int scrw, scrh;
        SDL_GetWindowSize(screen, &scrw, &scrh);

        SDL_Event event;
        int tdx = 0, tdy = 0;
        while(events.length() || SDL_PollEvent(&event)) {
            if(events.length()) event = events.remove(0);

            extern void menuevent(SDL_Event *event);
            if(touchmenuvisible()) {
                menuevent(&event);
                continue;
            }

            switch(event.type) {
                case SDL_KEYDOWN:
                case SDL_KEYUP:
                    // the volume-up key is currently the only physical key that can (optionally) be used on Android and so we only process keys if that's the case

                    if(volumeupattack)
                        keypress((int) event.key.keysym.sym, event.key.state==SDL_PRESSED, (int) event.key.keysym.sym, (SDL_Keymod) event.key.keysym.mod);

                    break;
                case SDL_TEXTINPUT:
                case SDL_TEXTEDITING:
                    extern void menuevent(SDL_Event *event);
                    if(touchmenuvisible()) {
                        menuevent(&event);
                        break;
                    }
                    break;

                case SDL_FINGERMOTION:
                    if(touchmenuvisible()) {
                        menuevent(&event);
                        break;
                    }

                    if(event.tfinger.fingerId != movementdirectionfinger)
                    {
                        if(event.type == SDL_FINGERMOTION)
                        {
                            int dx = event.tfinger.dx * scrw, dy = event.tfinger.dy * scrh;
                            entropy_add_byte(dx ^ (256 - dy));
                            checktouchmotion(dx, dy);
                            tdx += dx;
                            tdy += dy;

                            if(event.tfinger.fingerId == lookingdirectionfinger)
                            {
                                touch1->x = event.tfinger.x;
                                touch1->y = event.tfinger.y;
                            }
                        }

                        break;
                    }

                case SDL_FINGERDOWN:
                case SDL_FINGERUP: {
                    if(touchmenuvisible())
                    {
                        menuevent(&event);
                        break;
                    }

                    vector <keyevent> touchkeys = gettouchkeys(event);

                    // release previously pressed key of the given finger
                    vector<int> **lasttouchkeys = lastpressedtouchkeys.access(event.tfinger.fingerId);
                    if(lasttouchkeys) loopirev((*lasttouchkeys)->length())
                    {
                        int lasttouchkey = (*(*lasttouchkeys))[i];
                        bool found = false;
                        loopj(touchkeys.length())
                        {
                            if(lasttouchkey == touchkeys[j].key) {
                                found = true;
                                break;
                            }
                        }
                        if(!found)
                        {
                            LOGE("releasekey: %i", lasttouchkey);
                            keypress(lasttouchkey, false, 0);
                            (*lasttouchkeys)->pop();
                        }
                    }

                    // remember last pressed touch key
                    vector<int> *pressedtouchkeys = new vector<int>();
                    loopv(touchkeys)
                    {
                        if(touchkeys[i].type == SDL_FINGERDOWN ||
                           touchkeys[i].type == SDL_FINGERMOTION)
                        {
                            pressedtouchkeys->add(touchkeys[i].key);
                        }
                        LOGE("lastpressedtouchkeys inserted %i", touchkeys[i].key);
                    }
                    if(!pressedtouchkeys->empty())
                    {
                        lastpressedtouchkeys.access(event.tfinger.fingerId,
                                                    pressedtouchkeys); // fixme memory leak
                        lastpressedtouchkeys[event.tfinger.fingerId] = pressedtouchkeys; // fixme
                    }

                    loopv(touchkeys)
                    {
                        // filter our duplicate events as we want to process up/down events only once per finger and key
                        vector<keyevent*> **lastkeyevents = touchhistory.access(event.tfinger.fingerId);
                        if(lastkeyevents)
                        {
                            keyevent *lastkeyevent = NULL;
                            loopj((*lastkeyevents)->length())
                            if((**lastkeyevents)[j]->key == touchkeys[i].key &&
                               (**lastkeyevents)[j]->type == touchkeys[i].type)
                            {
                                lastkeyevent = (**lastkeyevents)[j];
                                break;
                            }
                            if(lastkeyevent)
                            {
                                LOGE("duplicate key filtered %i", touchkeys[i].key);
                                continue;
                            }
                        }

                        // fixme
                        if(touchkeys[i].key == -1) break;
                        if(event.tfinger.fingerId == movementdirectionfinger)
                        {
                            LOGE("keypress1: %i %s", touchkeys[i].key,
                                 (touchkeys[i].type == SDL_FINGERDOWN ||
                                  touchkeys[i].type == SDL_FINGERMOTION) ? "down" : "up");
                            keypress(touchkeys[i].key, (event.type == SDL_FINGERDOWN || event.type == SDL_FINGERMOTION), 0);
                        }
                        else {
                            bool isdown = touchkeys[i].type == SDL_FINGERDOWN;
                            LOGE("keypress1: %i %s", touchkeys[i].key, isdown ? "down" : "up");
                            keypress(touchkeys[i].key, isdown, 0);
                        }
                    }

                    // remember last key events
                    vector<keyevent*>*newkeyevents = new vector<keyevent *>();
                    loopv(touchkeys)
                    {
                        keyevent *e = new keyevent(touchkeys[i].type, touchkeys[i].key);
                        newkeyevents->add(e);
                    }
                    touchhistory[event.tfinger.fingerId] = newkeyevents;
                    break;
                }
            }
        }
        if(tdx || tdy)
        {
            entropy_add_byte(tdy + 5 * tdx);
            mousemove(tdx, tdy);
        }
    }

    // angle in 'center' between v1 and v2
    float anglebetween(vec &center, vec &v1, vec &v2)
    {
        return atan2(v2.y - center.y, v2.x - center.x) - atan2(v1.y - center.y, v1.x - center.x);
    }

    // apply motion of pointer from accumulated events and new events
    void checktouchmotion(int &dx, int &dy)
    {
        int scrw, scrh;
        SDL_GetWindowSize(screen, &scrw, &scrh);

        loopv(events)
        {
            SDL_Event &event = events[i];
            if(event.type != SDL_FINGERMOTION) {
                if(i > 0) events.remove(0, i);
                return;
            }
            dx += event.tfinger.dx * scrw;
            dy += event.tfinger.dy * scrh;
        }
        events.setsize(0);
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(event.type != SDL_FINGERMOTION) {
                events.add(event);
                return;
            }
            dx += event.tfinger.dx * scrw;
            dy += event.tfinger.dy * scrh;
        }
    }
};