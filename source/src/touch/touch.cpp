// touch.cpp: ingame user interface for mobile devices
// The goal is to ensure that the player has a great experience with the game on his mobile device even they never
// played a first person shooter before. Happy players will leave good ratings in appstore / playstore.
// This means on mobile we want to sacrifice functionality to maximize ease of use.
// Btw we are using an approach to eliminate cpp forward declarations, see http://strlen.com/java-style-classes-in-c/

#include "cube.h"

extern void trydisconnect();
extern void closetouchmenu();
extern void addnbot(const char *arg1, const char *arg2, const char *arg3);
extern void connectserv(char *servername, int *serverport, char *password);
extern void toggleconsole();
extern void voicecom(char *sound, char *text);
extern float sensitivity, mouseaccel;
extern void drawicon(Texture *tex, float x, float y, float s, int col, int row, float ts);
extern void turn_on_transparency(int alpha = 255);
extern const char *gunnames[];
extern vec *movementcontrolcenter;
extern float movementcontrolradius;
extern int hideconsole;
extern int developermode;
extern int numgamesplayed;
extern bool sentvoicecom_public;
extern bool sentvoicecom_team;

VARP(volumeupattack, 0, 0, 1); // determines if volume-up key should be bound to attack
VARP(onboarded, 0, 0, 1); // determines if the onboarding process has been completed
VARP(touchoptionsanimation, 0, 1, 4 ); // show touchui options 0:linear, ease1-4 // TODO: settings slider
VARP(touchoptionsanimationduration, 0, 500, 5000 ); // 0:off | TODO: settings slider - TODO @ RELEASE values more like: (0, 250, 750)

// ATM we only have voicecom options toggled in game@scoreboard scene
int touchoptionstogglemillis = 0; // one timer for all, toggling a second ON after a first will jump-start that one.
int touchoptionstogglestate = 0; // 0:off, 1:animating, 2:open

float movementcontrolradius = VIRTH/4;
vec *movementcontrolcenter;

struct touchui
{
    struct view;
    static vector<view *> viewstack;

    #include "config.h"
    static struct config config;

    #include "core/game.h"
    static struct game game;

    #include "core/input.h"
    static struct input input;

    #include "core/view.h"
    #include "core/navigationbutton.h"
    #include "core/textview.h"
    #include "core/touchmenu.h"
    #include "core/modelview.h"
    #include "core/sliderview.h"

    #include "core/hud.h"
    static struct hud hud;

    #include "mainscene/introscene.h"
    #include "mainscene/namescene.h"
    #include "mainscene/weaponscene.h"
    #include "mainscene/teamscene.h"
    #include "mainscene/skinscene.h"
    #include "mainscene/serverscene.h"
    #include "mainscene/mainscene.h"

    #include "gamescene/equipmentscene.h"
    #include "gamescene/settingsscene.h"
    #include "gamescene/helpscene.h"
    #include "gamescene/creditscene.h"
    #include "gamescene/gamescene.h"

    void openmainscene(bool servers)
    {
        mainscene *scene = new mainscene(NULL);
        scene->oncreate();
        if(servers || onboarded) scene->showserverscene();
        viewstack.add(scene);
    }

    void opengamescene()
    {
        gamescene *scene = new gamescene(NULL);
        scene->oncreate();
        viewstack.add(scene);
    }

    void openequipmentscene()
    {
        equipmentscene *scene = new equipmentscene(NULL);
        scene->oncreate();
        viewstack.add(scene);
    }

    void togglevoicecomoptions(){
        if( touchoptionstogglestate == 0 )
        {
            if(touchoptionsanimationduration==0)
            {
                touchoptionstogglestate = 2;
            }else{
                touchoptionstogglestate = 1;
                touchoptionstogglemillis = lastmillis;
            }
        }else{
            touchoptionstogglestate = 0; // OFF is w/o animation
        }
    }

    bool visible()
    {
        return viewstack.length();
    }

    void captureevent(SDL_Event *event)
    {
        if(!viewstack.length()) return;
        viewstack.last()->captureevent(event);
    }

    void render()
    {
        if(!viewstack.length()) return;
        viewstack.last()->renderroot();
    }

} touch;

vector<touchui::view *> touchui::viewstack;
struct touchui::game touchui::game;
struct touchui::config touchui::config;
struct touchui::input touchui::input;
struct touchui::hud touchui::hud;

// the game interacts with the touch UI through the exported functions below

bool touchenabled() { return touch.config.enabled; }
void showtouchmenu(bool servers) { touch.openmainscene(servers); }
void showgamemenu() { touch.opengamescene(); }
void showequipmentmenu() { touch.openequipmentscene(); }
void togglevoicecomoptions(){ touch.togglevoicecomoptions(); }
void menuevent(SDL_Event *event) { touch.captureevent(event); }
void rendertouchmenu() { touch.render(); }
bool touchmenuvisible() { return touch.visible(); }
void rendertouchhud(playerent *d) { touch.hud.draw(d); }
bool hijackvolumebuttons() { return volumeupattack && !touchmenuvisible(); }
bool allowaskrating() { return touchmenuvisible() && numgamesplayed >= 5; }

COMMAND(showgamemenu, "");
COMMAND(showequipmentmenu, "");
COMMAND(togglevoicecomoptions, "");

void checktouchinput() { touch.input.checkinput(); }