#!/bin/bash
# Adds a custom menuitem for AssaultCube to your Desktop's launcher.

CUBE_DIR=$(dirname "$(readlink -f "${0}")")
CUBE_EXEC=assaultcube.sh
LAUNCHERPATH="${HOME}/.local/share/applications/"

LAUNCHERFILE=assaultcube_dev.desktop
LAUNCHERTITLE="AssaultCube v1.2dev"

# Remove existing menuitem, if it exists:
EXISTINGEXEC=`find "${LAUNCHERPATH}" -name "assaultcube*" | xargs`
if [ "$EXISTINGEXEC" != "" ]; then
  echo "The following menuitem(s) currently exist:"
  echo "$EXISTINGEXEC"
  read -p "Would you like them all to be deleted? (y/N): " -r REPLY
  if [ "$REPLY" = "y" ] || [ "$REPLY" = "yes" ] || [ "$REPLY" = "Y" ] || [ "$REPLY" = "YES" ]; then
    find "${LAUNCHERPATH}" -name "assaultcube*" -delete
    echo "Deleted menuitems as requested." && echo ""
    exit 0
  else
    echo "The existing menuitems will remain." && echo ""
  fi
fi

mkdir -p "${LAUNCHERPATH}"
cat > "${LAUNCHERPATH}"${LAUNCHERFILE} << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=$LAUNCHERTITLE
Keywords=assaultcube;game;fps;
GenericName=First Person Shooter Game
Categories=Game;ActionGame;Shooter;
Terminal=false
StartupNotify=false
Exec=$CUBE_DIR/$CUBE_EXEC
Icon=$CUBE_DIR/packages/misc/icon.png
Comment=A multiplayer, first-person shooter game, based on the CUBE engine. Fast, arcade gameplay.
EOF

chmod +x "${LAUNCHERPATH}"${LAUNCHERFILE}
if [ -x "${LAUNCHERPATH}"${LAUNCHERFILE} ]; then
  echo "An AssaultCube menuitem has been successfully created at"
  echo "${LAUNCHERPATH}"${LAUNCHERFILE}
  exit 0
else
  echo "For some reason, we're unable to install an AssaultCube menuitem."
  exit 1
fi

