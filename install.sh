#!/bin/bash
#Configures the environment for the *nix users.

ac_version=1.1.0.5
ac_dir=$(dirname $(readlink -f "${0}"))
ac_exec=assaultcube.sh
ac_menupath=~/.local/share/applications
ac_desktoppath=~/Desktop
ac_entryfile=assaultcube.desktop
ac_desktop="undefined"

GetDesktop()
{
  case ${DESKTOP_SESSION} in
  gnome)
    ac_desktop="Gnome"
    ;;
  *)
    ac_desktop="undefined"
    ;;
  esac
}
existDesktopEntry()
{
  if [ -f ${ac_desktoppath}/${ac_entryfile} ]; then
    ac_existdesktopentry="X"
  else
    ac_existdesktopentry=" "
  fi
}
existMenuEntry()
{
  if [ -f ${ac_menupath}/${ac_entryfile} ]; then
    ac_existmenuentry="X"
  else
    ac_existmenuentry=" "
  fi
}
existProtocol()
{
  ac_existprotocol=" "
}
installDesktopEntry()
{
  mkdir -pv ${ac_desktoppath}
  cat > ${ac_desktoppath}/${ac_entryfile} <<EOF
[Desktop Entry]
Version=${ac_version}
Type=Application
Encoding=UTF-8
Exec=${ac_dir}/${ac_exec}
StartupNotify=false
Categories=Game;ActionGame;
Name=AssaultCube
Comment=Fast paced first-person shooter based upon the Cube engine
EOF
  chmod uga+x ${ac_desktoppath}/${ac_entryfile}
}
installMenuEntry()
{
  mkdir -pv ${ac_menupath}
  cat > ${ac_menupath}/${ac_entryfile} <<EOF
[Desktop Entry]
Version=${ac_version}
Type=Application
Encoding=UTF-8
Exec=${ac_dir}/${ac_exec}
StartupNotify=false
Categories=Game;ActionGame;
Name=AssaultCube
Comment=Fast paced first-person shooter based upon the Cube engine
EOF
}
installProtocol()
{
  echo ""
}
uninstallDesktopEntry()
{
  rm ${ac_desktoppath}/${ac_entryfile}
}
uninstallMenuEntry()
{
  rm ${ac_menupath}/${ac_entryfile}
}
uninstallProtocol()
{
  echo ""
}

GetDesktop
echo "Hello. You are about to install version ${ac_version} of AssaultCube. Your Deskop: ${ac_desktop}"

if [ \! -x "${CUBE_DIR}/${CUBE_EXEC}" ]; then
  echo ""
  echo "Could not find the AssaultCube base directory, move this script to the AC base"
  echo "directory and start it again."
  exit -1
fi

existDesktopEntry
existMenuEntry
existProtocol
echo ""
echo -e "Following components are already installed:\n"
echo -e "Desktop Entry\t[${ac_existdesktopentry}]"
echo -e "Menu Entry\t[${ac_existmenuentry}]"
echo -e "URL Protocol\t[${ac_existprotocol}]"

echo ""
echo "What would you like to do? install/uninstall/cancel [install]"
read ac_installoption
case ${ac_installoption} in
""|i|I|install|Install)
  echo "Do you want to create a shortcut on your Desktop? yes/no [yes]"
  read ac_componentoption
  case  ${ac_componentoption} in
  ""|y|Y|yes|Yes)
    installDesktopEntry
    ;;
  n|N|no|No)
    ;;
  *)
    echo "Unknown option: skipping!"
    ;;
  esac
  echo "Do you want to create a shortcut in your menu? yes/no [yes]"
  read ac_componentoption
  case  ${ac_componentoption} in
  ""|y|Y|yes|Yes)
    installMenuEntry
    ;;
  n|N|no|No)
    ;;
  *)
    echo "Unknown option: skipping!"
    ;;
  esac
  echo "Do you want your browser to handle assaultcube:// links? yes/no [yes]"
  read ac_componentoption
  case  ${ac_componentoption} in
  ""|y|Y|yes|Yes)
    installProtocol
    ;;
  n|N|no|No)
    ;;
  *)
    echo "Unknown option: skipping!"
    ;;
  esac
;;
u|U|uninstall|Uninstall)
  echo "Do you want to remove the shortcut from your Desktop? yes/no [no]"
  read ac_componentoption
  case  ${ac_componentoption} in
  y|Y|yes|Yes)
    uninstallDesktopEntry
    ;;
  ""|n|N|no|No)
    ;;
  *)
    echo "Unknown option: skipping!"
    ;;
  esac
  echo "Do you want to remove the shortcut from your menu? yes/no [no]"
  read ac_componentoption
  case  ${ac_componentoption} in
  y|Y|yes|Yes)
    uninstallMenuEntry
    ;;
  ""|n|N|no|No)
    ;;
  *)
    echo "Unknown option: skipping!"
    ;;
  esac
  echo "Do you want to remove the assaultcube:// link handler? yes/no [no]"
  read ac_componentoption
  case  ${ac_componentoption} in
  y|Y|yes|Yes)
    uninstallProtocol
    ;;
  ""|n|N|no|No)
    ;;
  *)
    echo "Unknown option: skipping!"
    ;;
  esac
  ;;
c|C|cancel|Cancel)
  exit 0
  ;;
*)
  echo "Unknown option: aborting!"
  exit -1
  ;;
esac
