// draws the portion of head-up-display (HUD) that is specific to touch devices
struct hud
{
    void drawtouchicon(float x, float y, int col, int row, int alpha = 255 / 2)
    {
        static Texture *tex = NULL;
        if(!tex) tex = textureload("packages/misc/touch.png", 3);
        if(tex) {
            turn_on_transparency(alpha);
            drawicon(tex, x, y, 120, col, row, config.TOUCHICONGRIDCELL);
            glDisable(GL_BLEND);
        }
    }

    const float c1 = 1.70158;
    const float c2 = c1*1.525;
    const float c3 = c1+1;
    const float c4 = (2*M_PI)/3;

    const float n1 = 7.5625;
    const float d1 = 2.75;

    float easeoutback(float x)
    {
        return 1 + c3 * pow(x-1,3) + c1 * pow(x-1,2);
    }

    float easeoutelastic(float x)
    {
        return x == 0 ? 0 : (x==1? 1 : ( pow(2,-10*x) * sin( (x*10-0.75)*c4 ) + 1 ) );
    }

    float easeoutbounce(float x)
    {
        if(x<1/d1)
        {
            return n1*x*x;
        }
        else if(x<2/d1)
        {
            return n1*(x-=1.5/d1)*x+0.75;
        }
        else if(x<2.5/d1)
        {
            return n1*(x-=2.25/d1)*x+0.9375;
        }
        else
        {
            return n1*(x-=2.625/d1)*x+0.984375;
        }
    }

    float easeinoutback(float x){
        return x < 0.5
               ? ((pow(2*x, 2)*((c2+1)*2*x-c2))/2)
               : ((pow(2*x-2, 2)*((c2+1)*(x*2-2)+c2)+2)/2);
    }

    void draw(playerent *p)
    {
        extern int touchoptionstogglestate;
        extern int touchoptionstogglemillis;
        extern int touchoptionsanimation;
        extern int touchoptionsanimationduration;
        static vec movementcontrolcenter = config.movementcontrolcenter();
        const int iconsize = config.HUD_ICONSIZE;
        const int edgeair = iconsize/2;
        // spacious layout? int icongridstepx = VIRTW/12; int icongridstepy = VIRTH/9;

        turn_on_transparency(255); // includes GL_ENABLE(GL_BLEND)

        drawtouchicon(edgeair, edgeair, 3, 1); // TOUCH_GAME_CORNER_TOP_LEFT - open settings scene

        if(menuvisible())
        {
            // if the scoreboard (classical menu) is shown provide for "between respawns" opportunities like equipment changes and voicecom
            int voicecomcornerx = VIRTW-iconsize-edgeair;
            int voicecomcornery = VIRTH-iconsize-edgeair;
            drawtouchicon(edgeair, voicecomcornery, 2, 2); // TOUCH_GAME_CORNER_BOTTOM_LEFT - open equipment scene
            drawtouchicon( voicecomcornerx, voicecomcornery, 4, 4, 255); // TOUCH_GAME_CORNER_BOTTOM_RIGHT - toggle voiceom touchui
            if( touchoptionstogglestate > 0 ){
                // the two "public" extend vertically, the two "team" horizontally.
                // order: pub1, pub2, team1, team2
                /*
                 * spacious layout:
                 * int endposx[4] = { voicecomcornerx, voicecomcornerx, voicecomcornerx - icongridstepx * 3, voicecomcornerx - icongridstepx * 5 };
                 * int endposy[4] = { voicecomcornery - icongridstepy * 3, voicecomcornery - icongridstepy * 5, voicecomcornery, voicecomcornery };
                 */
                int endposx[4] = { voicecomcornerx, voicecomcornerx, voicecomcornerx - iconsize * 2, voicecomcornerx - iconsize * 4 };
                int endposy[4] = { voicecomcornery - iconsize * 2, voicecomcornery - iconsize * 4, voicecomcornery, voicecomcornery };
                if( touchoptionstogglestate == 1 ){ // animating
                    float interval = ( lastmillis - touchoptionstogglemillis ) / ( 1.0 * touchoptionsanimationduration );
                    float midpoint = 0.0;
                    switch( touchoptionsanimation ){
                        case 0: midpoint = interval; break;
                        case 1: midpoint = easeoutback( interval ); break;
                        case 2: midpoint = easeoutelastic( interval ); break;
                        case 3: midpoint = easeoutbounce( interval ); break;
                        case 4: midpoint = easeinoutback( interval ); break;
                        default: break;
                    }
                    int midposx[4] = {
                        endposx[0]
                        , endposx[1]
                        , voicecomcornerx - (int)(midpoint * ( voicecomcornerx - endposx[2] ))
                        , voicecomcornerx - (int)(midpoint * ( voicecomcornerx - endposx[3] ))
                    };
                    int midposy[4] = {
                            (int)(voicecomcornery - midpoint * ( voicecomcornery - endposy[0] ))
                            , (int)(voicecomcornery - midpoint * ( voicecomcornery - endposy[1] ))
                            , endposy[2]
                            , endposy[3]
                    };
                    drawtouchicon( midposx[0], midposy[0], 4, 2, 255); // TOUCH_GAME_VOICCEOM_PUBLIC_1
                    drawtouchicon( midposx[1], midposy[1], 4, 3, 255); // TOUCH_GAME_VOICCEOM_PUBLIC_2
                    drawtouchicon( midposx[2], midposy[2], 2, 4, 255); // TOUCH_GAME_VOICCEOM_TEAM_1
                    drawtouchicon( midposx[3], midposy[3], 3, 4, 255); // TOUCH_GAME_VOICCEOM_TEAM_2
                    if( lastmillis > touchoptionstogglemillis + touchoptionsanimationduration ){
                        touchoptionstogglestate = 2;
                    }
                }else{ // >= 2 // finished
                    int available_public = sentvoicecom_public ? 128 : 255;
                    int available_team = sentvoicecom_team ? 128 : 255;
                    drawtouchicon( endposx[0], endposy[0], 4, 2, available_public); // TOUCH_GAME_VOICECOM_PUBLIC_1
                    drawtouchicon( endposx[1], endposy[1], 4, 3, available_public); // TOUCH_GAME_VOICECOM_PUBLIC_2
                    drawtouchicon( endposx[2], endposy[2], 2, 4, available_team); // TOUCH_GAME_VOICECOM_TEAM_1
                    drawtouchicon( endposx[3], endposy[3], 3, 4, available_team); // TOUCH_GAME_VOICECOM_TEAM_2
                }
            }
        }
        else
        {
            // icons drawn at the very top from left to right: weapon | reload | zoom

            // todo: we might need an icon to show the scoreboard on demand, but currently we have too many buttons on the screen already
            //drawtouchicon(VIRTW-3*iconsize/2, iconsize/2, 2, 2, 255); // TOUCH_GAME_CORNER_TOP_RIGHT // WIP/TESTING: a suicide button for quick access to scoreboard in dead state

            drawtouchicon(VIRTW*5/8 - iconsize/2, iconsize/2, 0, 0, 255/4); // TOUCH_GAME_RIGHTSIDE_TOP_0 - change weapn
            drawtouchicon(VIRTW*6/8 - iconsize/2, iconsize/2, 1, 0, 255/4); // TOUCH_GAME_RIGHTSIDE_TOP_1 - reload
            if(player1->weaponsel->type == GUN_SNIPER) drawtouchicon(VIRTW*7/8 - iconsize/2, iconsize/2, 3, 0, 255/4); // TOUCH_GAME_RIGHTSIDE_TOP_2 - scopezoom

            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            turn_on_transparency(255/4);

            // draw movement control at the left side of the screen
            static Texture *movetex = NULL;
            if(!movetex) movetex = textureload("packages/misc/touchmove.png", 3);
            if(movetex)
            {
                float x1 = movementcontrolcenter.x - movementcontrolradius;
                float y1 = movementcontrolcenter.y - movementcontrolradius;
                quad(movetex->id, x1, y1, movementcontrolradius*2.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            }

            // draw look control at the right hand side of the screen
            static Texture *looktex = NULL;
            if(!looktex) looktex = textureload("packages/misc/touchlook.png", 3);
            if(looktex)
            {
                float x1 = VIRTW*3/4 - movementcontrolradius;
                float y1 = VIRTH/2 - movementcontrolradius;
                quad(looktex->id, x1, y1, movementcontrolradius*2.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            }

            glDisable(GL_BLEND);
        }

        glEnable(GL_BLEND);
    }
};