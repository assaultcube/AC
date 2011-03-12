TO ENSURE YOU HAVE NO ERRORS WITH ASSAULTCUBE:
	* Don't edit, rename or delete ANY files in this directory.
	* Don't add files to this directory.


Developers notes for this folder:
=================================
When adding an "official" map, follow this to-do list after fixing the map up.

  1)  Remove un-needed data from *.cfg files by running this command:
           gawk '{sub(/\r$/,"")};1' INPUT_FILE.cfg | gawk -F'//' '{print $1}' | gawk '{sub(/[ \t]+$/, "")};1' | gawk NF > OUTPUT_FILE.cfg
      - This will convert all new-lines to UNIX format (instead of DOS format).
      - This will remove all comments, trailing spaces/tabs & blank lines in that file.
  2)  Add licensing and author information as comments in *.cfg file.
  3)  "Normalise" the maps map-message.
  4)  Add map to the map menu, and the CTF map menu if applicable.
  5)  Add to "securemaps.cfg".
  6)  Add authors (plus texture/skymap/model/etc authors) to team.html in the readme.
  7)  Add new textures/models/skymaps/etc to default_map_settings.cfg as comments.
      If you added a skymap, add it to the "test skymaps" menu also.
  8)  Add new mapmodels to the editing menu.
  9)  Update the maprot.
  10) Create bot waypoints.
  11) Create a "preview" for the map.
