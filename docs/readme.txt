-Actioncube Beta 1.0.- 
By Rabid Viper Developement

Actioncube is a team oriented total conversion of the cubeengine by Wouter van Oortmersson. (www.cubeengine.com) 

Set in a realistic-looking environment (as far as that´s possible with cube of course), while gameplay stays fast and arcade.

Keep in mind that this beta release aims at presenting and providing a the basic gameplay elements of Actioncube. We hope to expand it in the future by adding more original, unusual and fun weapons in the spirit of the "Action-" fashion of mods. 
We also consider a port to Sauerbraten (most likely to be called "Actionbraten") in wich we plan to further refine gameplay and visuals, and possibly adding a unique self made texture set.

See /docs/package_copyrights.txt
    /source/src/LICENSE
    /source/src/AUTHORS
    /source/src/README

for additional information.   

This document will not provide detailed information on the cube-engine itself, see 

http://www.cubeengine.com/readme.html

for further information. 

======================================================================================================

---- Before you Play ----

1. Setting your name

Open /config/autoexec config and edit the line 

/name unarmed

to 

/name yourname

to setup your name. 

THE AUTOEXEC FILE PROVIDES COMMENTS AND SUGGESTIONS FOR FURTHER CUSTOMIZATION

2. Setting resolution. 

The default resolution is 1024x768. If you need to change it, modify actioncube.bat accordingly.
-w sets width
-h sets height

In linux, simply pass those parameters in the command line. 


------------------------ Setup ---------------------- 

In game, press ESC to acces the menu. 
In the "team/player setup" submenu, you can choose your team, skin and crosshair. 

Every player spawns with 3 Weapons. 

Knife an Pistol are the standart outfitting of every player. 
You can chose the primary weapon you want to use in the outfit menu (key b)
Grenades must be picked up.

The next time you spawn, you will be given the weapon you chose.

----------------------- Joining a Game --------------

In game, press ESC and choose "Multiplayer" -> "Refresh Server List" 
Refresh the server list every few days, to add newly created servers to your list.

The masterserver will be queried and provide you with a list of servers. 

Once you updated your server list, you can use "Multiplayer" -> "Serverbrowser" 
Simply a server (up/downarrow) and press return.   

----------------------- Controls --------------------

W - Move forward
S - Move backwards
A - Sidestep left
D - Sidestep right

Space - Jump 

R - Reload 
 

Mouse1 - Attack
Mouse2 - Use Scope (if you have the Sniper Rifle equipped)

(!) CHECK autoexec.cfg FOR SCOPE CONFIGURATION OPTIONS (!)

Mousewheel Up/Down - Cycle weapons. 

1 - Primary Weapon
2 - Pistol
3 - Grenade
4 - Knife

You can also assign custom keys to specific weapons. 
Just modify your autoexec.cfg accordingly.

Example:

bind f "primary"
bind e "secondary"
bind c "melee"
bind q "grenade"

You will find suggestions for more ergonomical key setups in the autoexec.cfg

------------------------------------The Weapons ----------------------------------------------

-Swiss Tech Combat Blade DR-88-

A sharp and swift blade. Your last ressort when you ran out of Ammo and your best friend
in humiliating snipers.

-Mk-77 Semi-Automatic Pistol- 

You basic sidearm. A powerful resort weapon, yet typically less effective at range.
The small clip size is something you need to keep an eye on. 

-V-19 Combat Shotgun-: 

8-round semi-automatic shotgun.  This powerful weapon can shred your opponents to pieces-
if you´re close enough.

-A-Ard 10 Submachine Gun-

This one releases a deadly spray of bullets at a high rate of fire.
However, due to the short barrel and small caliber,
it´s manstopping power is limited, and effectiveness ceases at longer ranges.

-MTP-57 Assault Rifle- 

The large caliber deals a substantial amount of damage. A long barrel ensures accuracy even at high range. 
It can become difficult to control when shooting bursts of more than 3 bullets. 

-Precision Tech AD-81 Sniper Rifle-

Semi Automatic Sniper rifle, equipped with a 2x Scope. The large caliber can do a frightful amount of damage. 
Though naturally most accurate when used with the scope, the highly ergonomic design allows the experienced shooter to operate it from the hip as well. 

------------------------ Gamemodes ---------------------

-Team Deathmatch- 

Two teams fight each other. The team team that has the most frags at the end of the round will win.

-Team Survivor-

Round based team combat. If you die, you will have to wait until the end of the round until you can spawn again. Once all members of a team are dead, the surviving team will be awarded a point and the next round will start. 

- Last man Standing - 

Round based combat, no teams. The surviving player will win the round and be awarded a point. 

- Deathmatch -

no teams, everyone for himself. :P


----------------------------How to choose your crosshair-----------------------

We wanted to provide this functionality in the menus, but whatever.. it´s simple: 

Go to actioncube/packages/misc/crosshairs


The crosshair that is currently used is called "default.png" Simply rename that file (for example to "default_.png") 

then rename your desired crosshair to "default.png"

You might need to set /crosshairsize n to your prefered value (this also depends on your resolution)
To keep the value permanently, simply add 

crosshairsize n

to your autoexec.cfg.  (n stands for "numerical value") 

=====================================================================================================

----------------------------- Credits --------------------------------

Arghvark: Project Leader, Game Concept, Code, Prototyping

driAn   : Lead Coder, Webdesign

makkE   : Lead artist
          Models, Skins, Animations, Mapping, 2d-Art, some textures, Sound

Verbal  : QA, Advisor, Lead Tester

Nieb    : Models

Mitaman : Mapping, Ressources


Thanks to :

Wouter van Oortmersson (Cube engine)
JCDCP (Laptop model, company)
DCP   (Soundtrack)
Boeck (Ressources)
dtd (Hosting)

Texture Sources:

Ted Southard (DigitalFlux)
Noctua Graphics
Craig Fortune
Arcitool
3dcafe
Rohrschach
lemong.fr
Mayang.com
Chris Zastrow
imageafter.com
my box and photoshop


See /docs/package_copyrights.txt
    /source/src/LICENSE
    /source/src/AUTHORS
    /source/src/README

for additional information.    
