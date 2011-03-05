#!/bin/bash
#Configures the environment for the *nix users.

ac_version=1.1.0.5
ac_dir=$(dirname $(readlink -f "${0}"))
ac_exec=assaultcube.sh
ac_desktop="undefined"
ac_existdesktopentry=" "
ac_existmenuentry=" "
ac_existprotocol=" "

GetDesktop()
{
    ac_desktop="undefined"
}
existDesktopEntry()
{
    ac_existdesktopentry="X"
}
existMenuEntry()
{
    ac_existmenuentry="X"
}
existProtocol()
{
    ac_existprotocol="X"
}

GetDesktop
echo "Hello. You are about to install version $ac_version of AssaultCube. Your Deskop: $ac_desktop"

existDesktopEntry
existMenuEntry
existProtocol
echo ""
echo -e "Following components are already installed:\n"
echo -e "Desktop Entry\t[$ac_existdesktopentry]"
echo -e "Menu Entry\t[$ac_existmenuentry]"
echo -e "URL Protocol\t[$ac_existprotocol]"

echo ""
echo "What would you like to do? install/uninstall/cancel [install]"
read ac_installoption

#Desktop Entry

#Menu Entry

#URL Protocol
