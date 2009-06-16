#!/bin/sh
# announce the game to modern desktop environments
# (by creating ~/.local/share/applications/assaultcube_svn.desktop and ~/.local/share/icons/assaultcube.png)
# Hint: this script has to be located in the assaultcube base directory to work

# get Exec target path (CUBE_DIR has to be the full path of the assaultcube base directory)
CUBE_DIR=$(dirname $(readlink -f "${0}"))

if [ -x ${CUBE_DIR}/assaultcube.sh ]
then
  cd ${CUBE_DIR}
  mkdir -pv ~/.local/share/applications
  mkdir -pv ~/.local/share/icons
  cat  > ~/.local/share/applications/assaultcube_svn.desktop << EOF_DESKTOPFILE
[Desktop Entry]
Version=1.0
Encoding=UTF-8
Name=AssaultCube SVN Test Version
Type=Application
GenericName=First Person Shooter
Icon=assaultcube.png
StartupNotify=false
Categories=Game;ActionGame;
Comment=Fast paced first-person shooter based upon the Cube engine
Comment[pl]=Szybka strzelanka oparta na silniku gry Cube
Comment[de]=Schneller Ego-Shooter basierend auf der Cube-Engine
EOF_DESKTOPFILE
  echo "Exec=${CUBE_DIR}/assaultcube.sh" >> ~/.local/share/applications/assaultcube_svn.desktop
  cp ${CUBE_DIR}/docs/pics/assaultcube.png ~/.local/share/icons
else
  echo "could not find the AssaultCube base directory"
  echo "move this script to the AC base directory and start it again"
  exit 1
fi

