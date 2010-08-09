Official 'Cube with Bots' readme.

Current version: 0.32


AUTHOR:

Rick Helmus
E-Mail: rickhelmus@gmail.com
MSN: rick_helmus8@hotmail.com
ICQ: 224332897
AIM: rickakame




CONTENT

1. Features
2. Installing & Uninstalling
     2.1 Installing
     2.2 Uninstalling
3. Starting a game
4. Bot settings
     4.1 Bot names & teams
     4.2 Bot skills
     4.3 Bot configuration file
5. Bot commands & CVARS
     5.1 How to execute a command
     5.2 List of all server side commands
     5.3 List of all client side commands
     5.4 How to change a bot CVAR
     5.5 List of all bot CVARS
     5.6 List of all waypoint commands
6. Waypoints
     6.1 What are waypoints?
     6.2 When should I make my own waypoints?
     6.3 List of all waypoint commands
     6.4 How to use & Tips
7. Dedicated servers
8. Source code
9. Credits
10. Questions, suggestions, comments etc




1. FEATURES:

See the file 'changelog.txt' in your cube/bot folder.




2. INSTALLATION & UNSTALLATION


2.1 Installing

Extract all the files to your cube folder.
Don't worry it won't overwrite any cube specific files.

Note: Since the file structure changed a bit, it might be handy to uninstall previous cube bot versions first.

2.2 Uninstalling

If you want for some reason uninstall the bots you have to delete these files:
- The bot folder,
- bin/cubebot.exe
- 'cube_bot_linux',
- 'cube_bot_fbsd',
- 'cube_bot_win.bat',
- 'cube_bot_linux-ded',
- 'cube_bot_fbsd-ded',
- 'cube_bot_win-ded.bat'
- This file




3. STARTING A GAME

Run either cube_bot_win.bat (for windows users), cube_bot_fbsd(for FreeBSD users) or cube_bot_lin (for linux users).

For linux/unix users: if you have problems with running cube, you might want to 'chmod it'.
Type this in a console in your cube directory:
chmod +x ./cube_bot_lin

To add a bot you can simply open the main menu(esc) and open the bot menu, from there
you can add a bot in a team or change the default bot skill.
You can also use the addbot command(see section 6) to add a bot.

Bots work for every singleplayer and multiplayer mode. In singleplayer they will help you to fight monsters and finish a level.
In multiplayer they try to kill you instead ;-)

For dedicated servers see section 7.



4. BOT SETTINGS

4.1 Bot names and teams

When you add a bot and don't specifiy a name, the bot will pick a name from the file
'bot_names.txt' in the folder cube/bot. This is a plain text file, you can modify existing names
and add up to 150 names. Note that names in cube can't contain more than 16 characters.
When you add a bot and don't specify his team, it will pick one from the file 'bot_teams.txt'
found in the cube/bot folder. This file can contain 20 different team names with a maximal
length of 5 characters.

4.2 Bot skills

There are 5 skills for the bots. The skill for a bot can be specified from the menu or via the console(see section 6).
Each skill can be modified by editing the file 'bot_skills.txt' found in the bot folder.
4.3 Bot configuration file


5. BOT COMMANDS

5.1 How to execute a bot command

There are 2 ways to execute a (bot)command:
- Hit the chat key(default t) and type /<commandhere>
- Hit the console key(default ~) and type the command.


5.2 List of all server side bot commands

Server side commands(works if your the host, otherwise people have to vote for it):

addbot <teamname> <skill> <botname>
Adds a bot in the current game. Command arguments:

<teamname>:     Specifies the team for the bot, when you leave it blank or
                use "random" as team the bot will choose a team from the file
                'bot/bot_teams.txt'.
<skill>:        The skill of the bot; can be best, good, medium, worse, bad, default or random.
                When blank or "default" is used as skill, the bot will use skill good. When "random" is used,
                the bot will use a random skill.
<name>:         The name for this bot. When blank the bot will pick a name from the
                file 'bot/bot_names.txt'

Examples:
To add a bot with a random team, default skill(good) and a random name:
addbot
To add a bot in team red, default skill with the name Joe:
addbot red default Joe


addnbot <count> <teamname> <skill>
Adds multiple bots to the game. Command arguments:

<count>:        Number of bots to add.
<teamname>:     Specifies the team for the bots, when you leave it blank or
                use "random" as team the bot will choose a team from the file
                'bot/bot_teams.txt'.
<skill>:        The skill of the bot; can be best, good, medium, worse, bad, default or random.
                When blank or "default" is used as skill, the bot will use skill good. When "random" is used,
                the bot will use a random skill.


kickbot <botname>
Will kick the bot with the name <botname> from the server.

Example: To kick bot Joe from the server:
kickbot Joe


kickallbots
Will kick all bots from the server


botskill <botname> <skill>
Will change the skill of the bot with the name <botname> to <skill>.
<skill> can be best, good, medium, worse or bad.

Example:
To change the skill of the bot Joe to best:
botskill Joe best


botskillall <skill>
Will change the bot skill and the default skill to <skill>.
<skill> can be best, good, medium, worse or bad.


5.3 List of all client side commands(Works if you're a host or joined a server):


drawbeamtobots
Will draw some particles(same as the rifle) to all bots. This more used for
debugging to see where the bots are :-)


togglegrap
Will toggle the focus of the mouse in game. Normally you can use your mouse to look around,
when you type this command your mouse cursor is visible and can be used as normally. This is only
usefull when you run cube windowed ofcourse.


togglebotview <botname>
When used you will see what the bot with the name <botname> sees. Type it again
(with or without name) to return to the game(you will respawn).


5.4 How to change a bot cvar

This works like bot commands: First open the console (with the chat key or the console key),
then just type the variabele name and put the desired value after it.


5.5 List of all bot CVARS (Server side variabels, host only)

botsshoot 1|0
Enables or disables bot shooting. 1 is enabled, 0 is disabled. Default is 1.


idlebots 1|0
When enabled, all bots are idle(they won't 'think'). 1 is enabled, 0 is disabled. Default is 0.


maxbots 0-50
Specifies how many bots can be in a game. Ranges from 0 to 50, default is 7.
Note that there cannot be more bots than maxclients-1(so atleast one human can be in the game).


allowaddbotvotes 1|0
When enabled, clients are able to add bots through voting. 1 is enabled, 0 is disabled. Default is 1.


allowbotskillvotes 1|0
When enabled, clients are able to change the bots skill through voting. 1 is enabled, 0 is disabled.
Default is 1.


allowkickbotvotes 1|0
When enabled, clients are able to kick bots through voting. 1 is enabled, 0 is disabled.
Default is 1.


monstersperbot 1-25
Specifies how with how much the total monster count should be increased in DMSP mode. This number also depends on the skill:

nr of monsters = monster skill x monstersperbot cvar.

So for example, if monstersperbot is 7(default) and the monster skill is 8, the total monster count will be 7x8=56. 


monsterspawnmillis 1-2000
Specifies the delay in milliseconds for when new monsters should be added in DMSP mode. The default is 1000(=1 second).
Decrease this value if you want a monster war :) You should use it with caution though, since many monsters at the same time will
be very CPU intensive.
Note: The formula for the number of new monsters per spawn is: 1 + 1/3 of the number of bots ingame.
So for example, if monsterspawnmillis is 1000 and there are 3 bots ingame, there will be
1 + 1 (1/3 of 3 = 1) = 2 monsters that will spawn every second.




6. WAYPOINTS

6.1 What are waypoints?

Waypoints are used by bots to help them navigate through a map. A waypoint
is simply a spot in a map, by connecting them with other waypoints bots know how to
move from waypoint to waypoint. Note that waypoints can be connected 1 way or 2 way,
if a waypoint is connected 1 way this means that waypoint A can be connected with B, but
B is NOT connected to A.
The waypoints are stored in the directory bot/waypoints in your cube directory.

When waypoint viewing is enabled(see the 'wpvisible' command below) you can view all the waypoints
in the current map. Jump waypoints are red lines, the nearest normal waypoint
is a green line and all the others are blue lines. Connections between waypoints are
white lines. If a waypoint has a yaw(see the 'setwpyaw' command below) a short red line from the waypoint
in the direction taken from the yaw will be drawed.

6.2 When should I make my own waypoints?

If a map is loaded and no waypoints exist for this map, it will be flooded with waypoints(Automatically waypointed).
For most of the multiplayer maps this is enough(except complex maps like '32') but for SP(Single Player) maps
its better to make your own waypoints. This because these maps are generally more complex and there are one or more goals.

However the following SP maps are already waypointed by me or can be played without custom made waypoints:
- cruel01 & cruel02
- kksp1
- nsp2
- wsg1 & wsg2 & wsg3
- mpsp4

6.3 Waypoint commands

addwp
Adds a waypoint at the place where you are, all nearby waypoints will be connected with it.


delwp
Deletes the nearest waypoint.


wpsave
Saves the waypoints to a file.


wpload
Loads waypoints from a file, only handy when you want to undo your changes.(Waypoints will
be loaded automaticly when a map is started).


wpclear
Will clear all waypoints from the map. You usually want to use this to clear all automaticly generated
waypoints when you start making your own waypoints.


wpvisble 1|0
When enabled(1) you can view all the waypoint in the current map, 0 will
disable this. (Waypoints will be automaticly visible when you use any
waypoint command). Jump waypoints are red lines, the nearest normal waypoint
is a green line and others are blue lines. Connections between waypoints are
white lines.


autowp 1|0
Enables(1) or disables(0) autowaypointing. When enabled you will drop waypoints while
you walk around in the map.


wpinfo 1|0
If enabled(1)(disabled=0) you can view info(such as waypoint property flags, trigger numbers, yaw etc) from
a nearby waypoint.


addtelewps
Adds waypoints on all teleporters and their destination in the current map and connects
the waypoints at a teleporter with the waypoint at its destination.


addtriggerwps
Adds waypoints at all triggers(such as carrots and switches) in the current map.


setwptriggernr <nr>
Assigns a number to the nearest waypoint. This is only used for trigger waypoints, so that the bots go to triggers in the
right order. If you don't do this bots will search for every trigger, even when they are not reachable yet.


setwpyaw
Sets the yaw of this waypoint. If a bot reaches a waypoint with a given yaw, it equalize his yaw with the one from
the waypoint. This is usefull for very narrow spaces or jump waypoints.
The yaw is taken from the yaw of the player.


addpath1way1
Selects first waypoint to connect with another waypoint.


addpath1way2
Will connect the waypoint you selected with the command 'addpath1way1' with the
nearest waypoint.


addpath2way1, addpath2way2
Same as above, though it will connect both waypoints
with each other(that is there is a connection between the one
you select with 'addpath2way1' and the nearest waypoint AND there
is a connection between the nearest waypoint and the one you
selected with 'addpath1way1').


delpath1way1, delpath1way2, delpath2way1, delpath2way2
Same as above, but will remove connections between waypoints.


setjumpwp
Makes the nearest waypoint a "jump waypoint" this simply means that a bot will
jump when it reaches this waypoint(usefull when the bot has to jump over an obstacle
for example).


unsetjumpwp
Removes the jump flag from the nearest waypoint.


startflood
Starts flooding the map with waypoints. This is normally done when the game starts and no waypoints exist for the current map.


6.4 How to use & Tips

6.4.1 Simple howto

Lets say you want to make waypoints for map 'foo'. This map is a singleplayer map you found on Quadropolis(http://cube.snieb.com/).

Here are some simple steps to waypoint the map:
- Start the game(see section 3) and load the map. Since its a singleplayer map it may be better to load it in
  multiplayer mode, so you won't get killed during waypointing ;)
- Now since the map is automaticly waypointed, the best thing todo is to clear these waypoints first.
  Simply type 'wpclear' in the console to remove them.
- Map foo has several teleporters, therefore its handy to put some waypoints on them and their destination. Luckely there is a
  command which does that and will even connect the waypoint on the teleporter with the waypoint on the teleport-destination.
  Simply type 'addtelewps' in the console to add them.
- Like most maps, foo has several triggers to open doors, unlock monsters etc. The command 'addtriggerwps' will put waypoints on all
  the triggers for you.
- Now you can actually start waypointing. On simple, not complex places you can type 'autowp 1' so it will drop 
  waypoints (within a certain radius) where you walk and connect them(2 way) with each other. However on more complex places
  (for example a place where you have to climb up) its better to place them manually with the command 'addwp'.
- After thats its a good thing to givbe the 'trigger waypoints' a number. Bots will go to the trigger with the
  lowest number(except 0) first, then go to the next one and the next one etc etc. First type 'wpinfo 1' so you can see which
  waypoints are trigger waypoints and which not. Now go to the trigger waypoint which should be triggered first and type
  'setwptriggernr 1'. Then go to the next trigger waypoint and type 'setwptriggernr 2' and continue like that untill you're done.
  Note: If the order doesn't matter(very rare) you can skip this step.
- When you're done type 'wpsave' to save the waypoints, reload the map and add some bots to check if they can finish the map by
  their selves :-)

6.4.2 Waypoint tips

- If you start with new waypoints, its mostly better to type 'wpclear' in the console to remove the waypoints which where automaticly  
  generated. If, however, you just want to make some minor modifications you can leave them.
- When you waypint a singleplayer map, its better to load the map in multiplayer mode so you the monsters won't annoy you.
- On narrow spaces its better to manually waypoint with 'addwp' and disable autowaypointing('autowp 0').
- Always check if every waypoint is connected with the right waypoints. Waypoints can be connected with to
  much waypoints or not connected at all. The following commands can be used to solve this: addpath1way1, addpath1way2, addpath2way1,
  addpath2way2, delpath1way1, delpath1way2, delpath2way1 and delpath2way2.
- If a bot should jump somewhere you can set the jump flag with the command 'setjumpwp'. Its handy to set the direction to where
  the bot should jump to with the command 'setwpyaw'.




7. DEDICATED SERVERS

As of release 0.3 there is dedicated support! :) Though the standalone server binary is less light than the one
for cube. It also needs some additional directories/files:
- The bot folder
- The packages folder, however it only needs the map files so you can remove all the sounds and textures.
- Server script file

If you want to configure bot cvars, add a few bots on startup etc. You should edit the bot.cfg file in the bot folder.
Explanation can be found in the file itself.

You can start a server by executing on of the following scripts:
cube_bot_linux-ded (linux)
or
cube_bot_fbsd-server (FreeBSD)
or
cube_bot_win-ded.bat (Windows)




8. SOURCE CODE

You can find the modified source in the "bot/bot_src" directory. Atleast 90% off all the changes are
marked by me with comments(mostly something like 'added by Rick').




9. CREDITS

Wouter van Oortmerssen aka Aardappel and all the other people
that made cube as what it is now(see the cube readme).

Pierre-Marie Baty (PMB): for most of the current navigation code and other misc. code.
Check out his RACC bot for half-life at racc.bots-united.com

Botman : 'The father of all half-life bots'. He created the HPB bot(hpb-bot.bots-united.com) which
is the base for almost all half-life bots. A lot of ideas are inspired by him.

arghvark : Fix for ATI based gfx cards on linux.

Internet Nightmare : Optimizing waypoint file reading/writing

People at bots-united forums and cube forums: For feedback, suggestions, reporting bugs
and bot names :)



10. Questions, suggestions, comments etc

Feel free to send them to me :) (check top of file for contact info)
You can also visit the forums: http://forums.bots-united.com/forumdisplay.php?f=77
