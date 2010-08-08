To ensure you have no errors with AssaultCube.
Do not edit, rename or delete maps in this directory.
Do not add files to this directory.


Note for folder:
================
Handy linux command for keeping *.cfg sizes small:
	gawk '{sub(/\r$/,"")};1' INPUT_FILE | gawk -F'//' '{print $1}' | gawk '{sub(/[ \t]+$/, "")};1' | gawk NF > OUTPUT_FILE

* Converts all new-lines to UNIX format (instead of DOS format).
* Removes all comments in the file.
* Removes all trailing spaces/tabs.
* Removes all blank lines.

Remember to replace the words INPUT_FILE and OUTPUT_FILE in this script with the file.