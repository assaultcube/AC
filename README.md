## What is AssaultCube:Parkour?
AssaultCube is a **FREE**, multiplayer, first-person shooter game, based on the
[CUBE engine](http://cubeengine.com/cube.php).
AssaultCube:Parkour is a branch where the popular playing variant - often called GEMA - is to become an official part of the game.

## More info:

The new gamemode genre comes in two flavours to start with: normpark and freepark; npark and fpark for short.
The freepark variant may still be renamed, repurposed or removed - a flight of fancy made me add it as an option, 
the idea would be for there to be no weapons damage caused in this mode. Neither from yourself nor other players.

The gamemode relies on the the TRIGGER entity; it provides us with the capabilities required to structure the parkour.
Water is now deadly, so raise the waterlevel to fill a cube above the floor of your pits to make the peril real. 
Playerstarts should have the team attribute (#2) set to 11 and if you place some with attribute #3 index-tags then you can create safe-place respawns. You first playerstart needs to have attribute #3 equal 0, then just place a playerstart at each safe-place with attribute #3 counting up: 1, 2, 3, …
To make the engine realise the player has reached the safe-place you need to place a trigger. Triggers currently have four uses (set by attribute #2), all accept a radius in attribute #3:
 - 0:text – this is currently not implemented in a clean fashion; map config may not include ALIAS. But ATM a trigger:0 with attribute #1=1 will try to output a **parkour_text_1** alias.
 - 1:safe - if you reach this entity your safe-place marker will be set to whatever the attribute #1 of the entity is; unless your safe-place marker is already higher.
 - 2:points - you get the amount of points stored in attribute #1
 - 3:finished - marks the end of your journey


## Redistribution:

You may redistribute AssaultCube in any way the license permits, such as the
free unmodified distribution of AssaultCube's source and binaries. If you have
any doubts, you can look at the
[license](https://assault.cubers.net/docs/license.html).

