ASSAULTCUBE MAPPERS README


CONFIG FILES
------------
Before you start creating your own map, it's very important to CREATE YOUR OWN "MAPNAME".CFG !

How?
 - Copy the 'default_map_settings.cfg' from the config folder. Paste it into the 'packages/maps' 
   folder. Rename it to the same name that you will be using for you map (but leave the filename
   extension .cfg on the end. For example: ac_complex.cfg).

Why?
 - The 'config/default_map_settings.cfg' contains the default map settings, which get used if
   no independent config file for your map is located. This may change with every release and
   if it does, your map will be all screwed up and look strange!

How to use the map's config file
 - Config files execute commands needed to make your map work, for example: loading textures.

 - After creating your own config file, comment-out 'texture-lines' you won't use and uncomment 
   those you want. To change the skymap, comment the current and uncomment the one you want to use.

 - Make sure you take note of comments about mapmodel usage.


CAPTURE THE FLAG
----------------
CTF maps must be balanced and big enough.

   /newent ctf-flag 0 // Adds the CLA flag. You can use the menu to place them if it's more convenient for you.
   /newent ctf-flag 1 // Adds the RVSF flag. You can use the menu to place them if it's more convenient for you.
   ... place these in their respective team bases.


LADDERS
-------
Ladders are comprised of *TWO* peices. One part is the ladder mapmodel which looks like a ladder, 
the other is the "entity" which is an invisible force that let's you climb the mapmodel like a ladder.

How?
 - Choose the appropriate place for the mapmodel, then, pick the mapmodel to use from the mapmodels menu.
   To place the mapmodels orientation correctly, move 90 degrees to the left before placing it.

 - To place the "entity" for the mapmodel, so that you can climb the ladder, type: /newent ladder X
   Make sure you place the entity in the same place as the ladder. Replace X with how high the ladder is.


SPAWN POINTS
------------
 - There MUST be one start place for each (2) teams, and spawns for the non-team modes.

   /newent playerstart 0   // Adds a CLA spawn. You can use the menu to place them if it's more convenient for you.
   /newent playerstart 1   // Adds a RVSF spawn. You can use the menu to place them if it's more convenient for you.
   ...place these spawns in the respective "bases" of the teams.

   /newent playerstart 100 // Adds a FFA (free-for-all) spawn. These spawns are used for non-team modes.
   ...place these in FFA fashion all across the map. 


GENERAL TIPS
------------
Use solids wisely to allow for a map of reasonable size. There IS a limit to openess in AssaultCube. 
If you plan on making wide open maps, mapping for Sauerbraten may be more your style.
Due to more mapmodels and higher player/hudgun-polycounts, performance is very important.

Leave enough room for strafing. AssaultCube is intended to be a fast paced game, 
so enough space to strafe around is vital.

While building the rough layout, it is a good idea to spam the whole level with small, high contrast 
light sources (2/3/4 radius, 255 brightness) to simulate a highly detailed scene, regarding WQD levels.

Replace those lights when going into details (Quickest way is to type: /clearents light )

The current texture set for AssaultCube is somewhat sensitive to very bright light, a good effect can be 
achieved by using (7-12 radius, 100-180 brightness) lights to add some "ambient" lighting first.

Then use smaller (2-4 radius, 255 brightness) for the highlights (where there are lamp-posts/light-bulb 
mapmodels for example).

Please, don't just make the whole map as bright as can be, actually take some time to create contrast with lighting.

At the end of the texture list is a black/white crosses sort-of texture. 
DON'T use it, it is not a texture, in-fact, it is a lack of a texture. It looks ugly and shouldn't be used in any map, 
because it's there to show if textures and skins did not load properly.


OFFICIAL ASSAULTCUBE MAPS
-------------------------
If you want to create maps for the offical AssaultCube package, you need to read the following.
These requirements are necessary because the maps that come with any AssaultCube release must 
be of high-quality.

 - AssaultCube maps should look a bit grim/dusky.
   At the moment the texture set is by no means perfect, so choosing the right texture for the 
   job might take some time.

 - The theme should be in an industrial/realistic setting.

 - Be frugal with lights:
	- No heavy lights.
	- Dont use _some_ different colors for lights.
	- Remember: Too many entities can lag some older machines.

 - No dark corners/places for campers.
	 - This is subject to discussion. You should provide some sniper spots, but players must 
	   have a chance to fight back by means of tactics. Don't build "sniper forts".

 - Be frugal with mapmodels:
	 - Mapmodels slow down the gamespeed, that's a problem on older machines - place them 
	   well, without repeating.
	 - Don't use a mapmodel just because it's there, it must be some-what reasonable.
	 - Less is more if placed well ;-)

 - There must be more than one way to rush from the one place to the other, like (as example) CS maps.

 - Use of own textures and mapmodels:
	 - Feel free to include your own custom textures/mapmodels/skymaps, but be aware that they might 
	   need to be discussed with the Lead Artist (currently: makkE). Any custom stuff you want to 
	   use must either be totally free-to-use or created by yourself. You MUST include the original 
	   authors copyright notes, and have the authors permission to use it if required.
	 - The particular licensing of content created by yourself is your own choice.
	 - Desired average WQD levels are: 2000-3000 (leaves enough room for models).
	 - Your fullbright, basic map (undetailed, blank rooms), should by no means 
	   exceed 1000 WQD (ideally around 100-800).
	 - ABSOLUTE maximum WQD levels are: 5000 (Special cases might be subject to discussion. 
	   Reason for this WQD limit is that, anymore and the maps will lag on older machines).

 - Check the official maps that are already included in the distribution, your map should 
   have a layout somewhat simillar to this.

 - Check out the "general tips", because most of these apply for getting your map into the official package.

 - Defiently DON'T use mapmodels to make a 3rd floor. Creating a second floor can be okay if there isn't 
   a large amount of it, although it is NOT advised.

 - Avoid strange gimmicks, such as walk-through doors, as "strange" gimmicks will defiently not be accepted.
   Don't expect maps that aren't created for current game modes to be included.

 - Keep map themes available to a "general" audience. Many different people from different cultures and 
   countries, of different ages play AssaultCube, keep this in mind ;)