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
    ac_existdesktopentry=" "
}
existMenuEntry()
{
    ac_existmenuentry=" "
}
existProtocol()
{
    ac_existprotocol=" "
}

GetDesktop
echo "Hello. You are about to install version ${ac_version} of AssaultCube. Your Deskop: ${ac_desktop}"

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
