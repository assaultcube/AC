#!/bin/bash
# A simple tool-script to change things in the ./config folder with ease.

# Run "sh configfolder_script.sh --all" to auto-accept "yes" as the answer to
# all questions in this script.
#===============================================================================

# This is the main AssaultCube folder.
PATHTOACDIR=~/AssaultCube/SVN_Trunk

# This is the docs folder (which holds reference.xml)
ABSOLUTEPATHTODOCS=~/AssaultCube/SVN_Website/htdocs/docs



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
  echo -e "Generated a new ./config/securemaps.cfg file."
else
  echo -e "\a\E[1mNOTE:\E[0m ./config/securemaps.cfg hasn't been updated."
fi

echo "Generate an updated ./config/docs.cfg (Y/N)?"
if [ "$1" != "--all" ]; then
  read ANSR
fi
if [ "$ANSR" = "y" ] || [ "$ANSR" = "Y" ] || [ "$ANSR" = "yes" ] || [ "$ANSR" = "YES" ] || [ "$1" = "--all" ]; then
  cd $ABSOLUTEPATHTODOCS
  xsltproc -o $PATHTOACDIR/config/docs.cfg ./xml/cuberef2cubescript.xslt ./reference.xml
  echo -e "Generated a new ./config/docs.cfg file."
else
  echo -e "\a\E[1mNOTE:\E[0m ./config/docs.cfg hasn't been updated."
fi

echo "Strip all \"official\" maps configs of cruft, leaving top-of-file comments alone (Y/N)?"
if [ "$1" != "--all" ]; then
  read ANSR
fi
if [ "$ANSR" = "y" ] || [ "$ANSR" = "Y" ] || [ "$ANSR" = "yes" ] || [ "$ANSR" = "YES" ] || [ "$1" = "--all" ]; then
  cd $PATHTOACDIR
  sh config/convert_mapconfig.sh -osp ./packages/maps/official/*.cfg
else
  echo -e "\a\E[1mNOTE:\E[0m Map config files have been left alone."
fi

echo "Strip all config files of trailing spaces/tabs (Y/N)?"
if [ "$1" != "--all" ]; then
  read ANSR
fi
if [ "$ANSR" = "y" ] || [ "$ANSR" = "Y" ] || [ "$ANSR" = "yes" ] || [ "$ANSR" = "YES" ] || [ "$1" = "--all" ]; then
  cd $PATHTOACDIR/config
  sed -i 's/^M$//' *
  sed -i 's/[ \t]*$//' *
else
  echo -e "\a\E[1mNOTE:\E[0m Config files haven't had leading whitespace stripped."
fi

echo "Update all menus with current \"official\" maps (Y/N)?"
if [ "$1" != "--all" ]; then
  read ANSR
fi
if [ "$ANSR" = "y" ] || [ "$ANSR" = "Y" ] || [ "$ANSR" = "yes" ] || [ "$ANSR" = "YES" ] || [ "$1" = "--all" ]; then
  cd $PATHTOACDIR/config

  # Auto-generates a list of maps, DO NOT change:
  MAPSLIST=`cd $PATHTOACDIR && find ./packages/maps/official/*.cgz |  xargs -i basename {} .cgz | tr '\n' ' '`

  # Start and end text for "const defaultmaps":
  SDEFMAPS="const defaultmaps [ "
  EDEFMAPS="]"
  # Start and end text for the bot silders:
  SBOTSLIDE='menuitemslider [Map: ] 0 (- (listlen $defaultmaps) 1) "$survMap" 1 [ '
  EBOTSLIDE='] [ survMap = $arg1 ]'
  # Start and end text for "const ctfmaps":
  SCTFMAPS="const ctfmaps [ "
  ECTFMAPS="]"

  # Currently listed CTF maps:
  CURCTFMAPS=`grep "const ctfmaps" menus.cfg | sed 's/const ctfmaps \[//g' | sed 's/\]//g'`
  # Make a list of CTF maps, with new additions:
  CTFOUT=`echo $CURCTFMAPS $NEWCTFMAPS | tr " " "\n" | sort -u | tr "\n" " "`

  sed -i "s/const defaultmaps..*/$SDEFMAPS$MAPSLIST$EDEFMAPS/g" menus.cfg
  sed -i "s/menuitemslider \[Map: \] 0..*/$SBOTSLIDE$MAPSLIST$EBOTSLIDE/g" menus_bot.cfg
  echo "The following official maps are listed for CTF mode:"
  echo $CURCTFMAPS
  echo "The following official maps are currently available:"
  echo $MAPSLIST
  echo "Add a map to this list (Y/N)?"
  if [ "$ANSR" = "y" ] || [ "$ANSR" = "Y" ] || [ "$ANSR" = "yes" ] || [ "$ANSR" = "YES" ]; then
    echo "Please type the names of maps to add to the CTF menu, seperated by spaces."
    read NEWCTFMAPS
    sed -i "s/const ctfmaps..*/$SCTFMAPS$CTFOUT$ECTFMAPS/g" menus.cfg
  else
    echo -e "\a\E[1mNOTE:\E[0m No changes have been made to the CTF map list."
  fi
else
  echo -e "\a\E[1mNOTE:\E[0m All map menus have been updated."
fi




