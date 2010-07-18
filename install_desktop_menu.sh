#!/bin/sh
# announce the game to modern desktop environments
# (by creating ~/.local/share/applications/assaultcube.desktop)
# Hint: this script has to be located in the assaultcube base directory to work

# get Exec target path (CUBE_DIR has to be the full path of the assaultcube base directory)

CUBE_DIR=$(dirname $(readlink -f "${0}"))
CUBE_EXEC=assaultcube.sh
SHORTCUTPATH=~/.local/share/applications
SHORTCUTFILE=assaultcube_svn.desktop

if [ -x "${CUBE_DIR}/${CUBE_EXEC}" ]; then
  if [ -f ${SHORTCUTPATH}/${SHORTCUTFILE} ]; then
    rm ${SHORTCUTPATH}/${SHORTCUTFILE}
    echo "Shortcut removed."
  else
    mkdir -pv ${SHORTCUTPATH}
    cat > ${SHORTCUTPATH}/${SHORTCUTFILE} <<EOF
[Desktop Entry]
Version=1.1
Type=Application
Encoding=UTF-8
Exec=${CUBE_DIR}/${CUBE_EXEC}
Icon=${CUBE_DIR}/docs/pics/assaultcube.png
StartupNotify=false
Categories=Game;ActionGame;
Name=AssaultCube SVN
Comment=Fast paced first-person shooter based upon the Cube engine
Comment[pl]=Szybka strzelanka oparta na silniku gry Cube
Comment[de]=Schneller Ego-Shooter basierend auf der Cube-Engine
EOF
    echo "Shortcut created."
  fi
else
  echo "Could not find the AssaultCube base directory, move this script to the AC base"
  echo "directory and start it again."
  exit -1
fi

