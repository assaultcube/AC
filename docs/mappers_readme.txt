::ActionCube Mappers Readme::

Before you start mapping, MAKE YOUR OWN <MAPNAME>.CFG !
The config/default_map_settings.cfg is only an example and might change with every release,
so you might lose the layout of your map !

Use the defaul_map_settings.cfg as base for your own .cfg, comment'texture-lines' you won't use and uncomment those you want.
Choose which skymap you d'like, etc.
Note the comments on particular mapmodel´s usage

.:Maps for the ActionCube Project:

If you want send us the maps you will make, you need to read the following lines.
These rules/definitions are necessary because the maps that come with an
ActionCube release must be of high-quality.

- AC maps should look a bit grim, dusky .

At the moment the texture set is by no means perfect,
so choosing the right texture for the job might take some time.

- The theme should be industrial/realistic setting

- Be frugal with lights:
	- no heavy lights
        - dont use _some_ different colors for lights
               
- No dark corners/places for campers*

* this is subject to discussion - you should provide sniper spots, but players must have a chance to   fight back by means of tactics. Don´t build "sniper forts".

- Be frugal with mapmodels:
	- mapmodels slow down the gamespeed, thats a problem on older machines - place them well,
          without repeating
        - don´t use a mapmodel just because it´s there, it must be somewhat reasonable 
        -less is more if placed well ;)

- There _must_ be one start place for each (2) teams, and spawns for the non-team modes. 

newent playerstart 0   //adds a CLA spawn
newent playerstart 1   //adds a RVSF spawn

-place these in the respective "base-areas" of the teams. 

newent playerstart 100 //adds a ffa spawn

-place these in ffa fashion all across the map. 

- CTF

CTF maps must be balanced and big enough. 

/newent ctf-flag 0 //adds the CLA flag
/newent ctf-flag 1 //adds the RVSF flag

Ladders

Add the appropriate ladder model, then 

/newent ladder

Just check the exsisting maps for correct placement.  


The map should be 'a line' between these place, there must be more than	one way to rush from the one place to the other,
like (as example) cs maps.

- Own textures and mapmodels:

feel free to include custom textures/mapmodels,
but be aware that they might need to be	discussed with the Lead Artist.
Any custom stuff you want to use must either be totally free to use or made by yourself.
You must include the original authors copyright notes, and have author´s permission to use if required.
The particular licensing of content created by yourself is your choice.

- Desired average wqd: 2000-3000 (leaves enough room for models) Your fullbright basic level (blank rooms)   Should by no means exceed 1000 wqd, ideally around 100-800)	
  Absolute maximum wqd  :5000 (special cases might be subject to discussion).

-> Check the maps that are already included, your map should have a layout somewhat simillar to this.

General Tips :

Use solids wisely to allow for a map of reasonable size. There IS a limit to openness in cube. If you plan on making wide open maps, you might consider to make maps for sauerbraten instead.
Due to more mapmodels and higher player/hudgun-polycounts performance is very important.

Leave enough room for strafing. Intended to be a fast paced game enough space to strafe around is vital.                          
While building the rough layout it is a good idea to spam the whole level with small, high contrast light sources
(2/3/4 255 lights) to simulate a highly detailed scene regarding wqd count.
Replace those lights when going into details.

The current texture set is somewhat sensitive to very bright light, a good effect can be achieved by using
(7-12  100-180) raduis/strengh  lights to add some "ambient" lighning first ,
and use smaller 2/3 255 for the highlights (where there are lamps/lamp models for example) 
By no means use the L button please. :)                             






