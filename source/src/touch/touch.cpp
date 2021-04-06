// touch.cpp: ingame user interface for mobile devices
// The goal is to ensure that John Doe has a great experience with the game on his mobile device even if Joe never
// played a first person shooter before. Happy Joe will leave a good rating on appstore / playstore.
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

VARP(volumeupattack, 0, 0, 1); // determines if volue-up key should be bound to attack
VARP(onboarded, 0, 0, 1); // determines if the onboarding process has been completed

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
void menuevent(SDL_Event *event) { touch.captureevent(event); }
void rendertouchmenu() { touch.render(); }
bool touchmenuvisible() { return touch.visible(); }
void rendertouchhud(playerent *d) { touch.hud.draw(d); }
bool hijackvolumebuttons() { return volumeupattack && !touchmenuvisible(); }
bool allowaskrating() { return touchmenuvisible() && numgamesplayed >= 5; }

COMMAND(showgamemenu, "");
COMMAND(showequipmentmenu, "");

void checktouchinput() { touch.input.checkinput(); }