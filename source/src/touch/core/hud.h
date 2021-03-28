// draws the portion of head-up-display (HUD) that is specific to touch devices
struct hud
{
    void drawtouchicon(float x, float y, int col, int row, int alpha = 255 / 2)
    {
        static Texture *tex = NULL;
        if(!tex) tex = textureload("packages/misc/touch.png", 3);
        if(tex) {
            turn_on_transparency(alpha);
            drawicon(tex, x, y, 120, col, row, 1 / 4.0f);
            glDisable(GL_BLEND);
        }
    }

    void draw(playerent *p)
    {
        static vec movementcontrolcenter = config.movementcontrolcenter();
        const int iconsize = config.HUD_ICONSIZE;

        if(menuvisible())
        {
            // if the scoreboard (classical menu) is shown then only draw the menu icon at the top left corner so that players can change weapon and team
            turn_on_transparency(255);
            drawtouchicon(iconsize/2, iconsize/2, 3, 1, 255/2); // TOUCH_GAME_LEFTSIDE_TOP_CORNER
            glDisable(GL_BLEND);
        }
        else
        {
            turn_on_transparency(255);

            // icons drawn at the very top from left to right: menu | weapon | reload | crouch | zoom
            drawtouchicon(iconsize/2, iconsize/2, 3, 1, 255); // TOUCH_GAME_LEFTSIDE_TOP_CORNER
            drawtouchicon(VIRTW*5/8 - iconsize/2, iconsize/2, 0, 0, 255/4); // TOUCH_GAME_RIGHTSIDE_TOP_0
            drawtouchicon(VIRTW*6/8 - iconsize/2, iconsize/2, 1, 0, 255/4); // TOUCH_GAME_RIGHTSIDE_TOP_1
            if(player1->weaponsel->type == GUN_SNIPER) drawtouchicon(VIRTW*7/8 - iconsize/2, iconsize/2, 3, 0, 255/4); // TOUCH_GAME_RIGHTSIDE_TOP_2

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