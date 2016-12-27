#!/bin/bash
# A simple tool-script to change things in the ./config folder with ease.

# Run "sh configfolder_script.sh --all" to auto-accept "yes" as the answer to
# all questions in this script.

# This is the main AssaultCube folder:
PATHTOACDIR=~/AssaultCube/AC

# This is the docs folder (which holds reference.xml):
ABSOLUTEPATHTODOCS=$PATHTOACDIR/docs

# Path to "official" folder:
MAPSPATH="$PATHTOACDIR/packages/maps/official"


echo "Generate an updated ./config/securemaps.cfg (Y/N)?"
if [ "$1" != "--all" ]; then
  read ANSR
fi
if [ "$ANSR" = "y" ] || [ "$ANSR" = "Y" ] || [ "$ANSR" = "yes" ] || [ "$ANSR" = "YES" ] || [ "$1" = "--all" ]; then
  cd $PATHTOACDIR
  echo "resetsecuremaps" > ./config/securemaps.cfg
  find ./packages/maps/official/*.cgz | \
    xargs -i basename {} .cgz | \
    xargs -i echo "securemap" {} | \
    sort -u >> ./config/securemaps.cfg
  echo -e "DONE.\n"
else
  echo -e "\a\E[1mNOTE:\E[0m ./config/securemaps.cfg hasn't been updated.\n"
fi

echo "Generate an updated ./config/docs.cfg (Y/N)?"
if [ "$1" != "--all" ]; then
  read ANSR
fi
if [ "$ANSR" = "y" ] || [ "$ANSR" = "Y" ] || [ "$ANSR" = "yes" ] || [ "$ANSR" = "YES" ] || [ "$1" = "--all" ]; then
  cd $ABSOLUTEPATHTODOCS
  xsltproc -o $PATHTOACDIR/config/docs.cfg ./xml/cuberef2cubescript.xslt ./reference.xml
  echo -e "DONE.\n"
else
  echo -e "\a\E[1mNOTE:\E[0m ./config/docs.cfg hasn't been updated.\n"
fi

echo "Strip all \"official\" maps configs of cruft, leaving top-of-file comments alone (Y/N)?"
if [ "$1" != "--all" ]; then
  read ANSR
fi
if [ "$ANSR" = "y" ] || [ "$ANSR" = "Y" ] || [ "$ANSR" = "yes" ] || [ "$ANSR" = "YES" ] || [ "$1" = "--all" ]; then
  cd $PATHTOACDIR
  bash config/convert_pre_v1.2_mapconfig.sh -osp ./packages/maps/official/*.cfg
  echo -e "DONE.\n"
else
  echo -e "\a\E[1mNOTE:\E[0m Map config files have been left alone.\n"
fi

echo "Strip all config files of trailing spaces/tabs (Y/N)?"
if [ "$1" != "--all" ]; then
  read ANSR
fi
if [ "$ANSR" = "y" ] || [ "$ANSR" = "Y" ] || [ "$ANSR" = "yes" ] || [ "$ANSR" = "YES" ] || [ "$1" = "--all" ]; then
  cd $PATHTOACDIR/config
  sed -i 's/^M$//' *.cfg *.txt
  sed -i 's/[ \t]*$//' *.cfg *.txt
  cd $PATHTOACDIR/config/autostart
  sed -i 's/^M$//' *.cfg *.txt
  sed -i 's/[ \t]*$//' *.cfg *.txt
  cd $PATHTOACDIR/config/opt
  sed -i 's/^M$//' *.cfg *.txt
  sed -i 's/[ \t]*$//' *.cfg *.txt
  echo -e "DONE.\n"
else
  echo -e "\a\E[1mNOTE:\E[0m Config files haven't had leading whitespace stripped.\n"
fi


# Auto-generates a list of maps:
MAPSLIST=`cd "$MAPSPATH" && find ./*.cgz |  xargs -i basename {} .cgz | sort -u | sed 's/\n/ /g'`

# Currently listed CTF maps:
CURCTFMAPS="$(cd $PATHTOACDIR/config && sed -n 's/const ctfmaps \[//p' menus.cfg | sed 's/\]//g')"

# List of non-CTF maps:
NONCTFLIST=`echo " " "$CURCTFMAPS" " " "$MAPSLIST" " " | sed "s/ /\n/g" |  sed '/^$/d' | sort | uniq -u`


read
echo "Update all menus with current \"official\" maps (Y/N)?"
if [ "$1" != "--all" ]; then
  read ANSR
fi
if [ "$ANSR" = "y" ] || [ "$ANSR" = "Y" ] || [ "$ANSR" = "yes" ] || [ "$ANSR" = "YES" ] || [ "$1" = "--all" ]; then
  cd $PATHTOACDIR/config

  # Replacement text for "const defaultmaps":
  DEFLTMAPS=`echo "const defaultmaps [" $MAPSLIST "]"`

  sed -i 's/const defaultmaps..*/'"$DEFLTMAPS"'/g' menus.cfg
  echo "The following official maps are NOT listed for CTF mode currently:"
  echo $NONCTFLIST
  echo "Add a map to this list (Y/N)?"
  read ANSR
  if [ "$ANSR" = "y" ] || [ "$ANSR" = "Y" ] || [ "$ANSR" = "yes" ] || [ "$ANSR" = "YES" ]; then
    echo "Please type the names of maps to add to the CTF menu, seperated by spaces."
    read NEWCTFMAPS

    # List of CTF maps, with new additions:
    CTFLIST="$(echo " " "$CURCTFMAPS" " " "$NEWCTFMAPS" " " | sed "s/ /\n/g" |  sed '/^$/d' | sort -u | sed "s/\n/ /g")"
    # Replacement text for "const ctfmaps":
    CTFMAPS="$(echo "const ctfmaps [" $CTFLIST "]")"

    sed -i 's/const ctfmaps..*/'"$CTFMAPS"'/g' menus.cfg
    echo -e "DONE.\n"
  else
    echo -e "\a\E[1mNOTE:\E[0m DONE... no changes were made to the CTF maps list."
  fi
else
  echo -e "\a\E[1mNOTE:\E[0m No map menus have been updated."
fi


