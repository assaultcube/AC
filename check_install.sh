#!/bin/sh
# Launches AssaultCube, configures an AC server, or installs/removes an AC menuitem.
# This file remains compatible with Bourne Shell, in case it is used on another *nix system.

# AC Version, this suffix is used to name the userdata directory and desktop launcher.
ACVERSION="v1.2"
# Default server options:
DEFSERVER="-Cconfig/servercmdline.txt"
# Default client options:
DEFCLIENT="--home=${HOME}/.assaultcube_v1.2 --init"

# These two aliases below are automatically chosen later in this script.
# You can specify them here though, if you would prefer.
#
# Path to the AssaultCube directory:
CUBEDIRC=""
# Name of the AssaultCube executable:
CUBEEXEC=""

# --help document
for cmdoptions in $@; do
  if [ "$cmdoptions" = "--help" ]; then
    echo "Usage: "$0" OPTION(S)"
    echo ""
    echo "The AssaultCube launcher/menuitem installer. AssaultCube is a multiplayer,"
    echo "first-person shooter game, based on the CUBE engine, with fast, arcade gameplay."
    echo ""
    echo "If none of the below options are chosen, this script will launch an AssaultCube"
    echo "client and any arguments from http://assault.cubers.net/docs/commandline.html"
    echo "will be added to AssaultCube's launch."
    echo ""
    echo "Note: Except for --basedirectory and --defaultsoff the below commands can't be"
    echo "      used conjunctionally with one another."
    echo ""
    echo "Options:"
    echo "--help                  This help message."
    echo "--installmenuitem       Installs (or overwrites an existing "$ACVERSION") menuitem for"
    echo "                        the current user (only), then exits. Any arguments used"
    echo "                        from http://assault.cubers.net/docs/commandline.html"
    echo "                        will be added to the launcher."
    echo "--removemenuitem        Removes all menuitems for the current user, then exits."
    echo "--basedirectory=value   Specifies the absolute path to AssaultCube's base"
    echo "                        directory. Useful if you're running this script from a"
    echo "                        different directory."
    echo "--installserver=value   Helps you configure an AssaultCube server launcher. If"
    echo "                        no \"value\" is chosen, the launcher file will be named"
    echo "                        \"serverlauncher\"."
    echo "--runserver             Runs an AssaultCube server with the default"
    echo "                        configuration found in ./config/servercmdline.txt, plus"
    echo "                        whatever you specify yourself on this commandline (not"
    echo "                        recommended - preferably use --installserver instead)."
    echo "--defaultsoff           Turns off ./config/servercmdline.txt options, when using"
    echo "                        --runserver, or turns off --init and --home when using"
    echo "                        an AssaultCube client."
    exit 0
  fi
done

for cmdoptions in $@; do
  # Store all arguments that aren't part of this script:
  TEMP1=`echo "$ACARGUMENTS" "$cmdoptions" | sed 's/--installmenuitem.*\|--removemenuitem.*\|--basedirectory.*\|--installserver.*\|--runserver.*\|--defaultsoff.*\|//g'`
  ACARGUMENTS=`echo "$TEMP1"`
  # Turn off the defaults if this was requested:
  if [ "$cmdoptions" = "--defaultsoff" ]; then
    DEFCLIENT="" && DEFSERVER=""
  fi
done

# Work out the absolute path to the AssaultCube directory.
# This will skip if it's already chosen (see head of script).
if [ -z "$CUBEDIRC" ]; then
  # Check if specified via switch.
  for cmdoptions in $@; do
    TEMP1="$(echo "$cmdoptions" | sed -n 's/--basedirectory=//p')"
    if [ -n "$TEMP1" ]; then
      CUBEDIRC="$(readlink -f "$(eval echo $TEMP1)")"
    fi
  done
  # Or, use current directory.
  if [ "$CUBEDIRC" = "" ]; then
    CUBEDIRC="$(dirname "$(readlink -f "${0}")")"
  fi
fi

# Work out if we're using a server or client binary.
for cmdoptions in $@; do
  TEMP1="$(echo "$cmdoptions" | grep -e --installserver)"
  TEMP2="$(echo "$cmdoptions" | grep -e --runserver)"
  if [ -n "$TEMP2" ]; then
    EXECTYPE="server"
  fi
  if [ -n "$TEMP1" ]; then
    EXECTYPE="server"
    # If --installserver was used, prepare the value to be used.
    SERVERLAUNCHER="$(echo "$TEMP1" | sed -n 's/--installserver=//p')"
    if [ -z "$SERVERLAUNCHER" ]; then
      SERVERLAUNCHER="serverlauncher"
    fi
  fi
done
if [ "$EXECTYPE" != "server" ]; then
  EXECTYPE="client"
fi

# Work out what binary our processor/OS requires.
# This will skip if it's already chosen (see head of script).
if [ -z "$CUBEEXEC" ]; then
  if [ -x "$CUBEDIRC"/bin_unix/native_"$EXECTYPE" ]; then
    CUBEEXEC="native_"$EXECTYPE""
  elif [ "$(uname -s)" = "Linux" ]; then
    if [ "$(uname -m)" = "x86_64" ]; then
      CUBEEXEC="linux_64_"$EXECTYPE""
    else
      CUBEEXEC="linux_"$EXECTYPE""
    fi
  else
    if [ "$(uname -m)" = "x86_64" ]; then
      CUBEEXEC="unknown_64_"$EXECTYPE""
    else
      CUBEEXEC="unknown_"$EXECTYPE""
    fi
  fi
fi

# Remove existing menuitems, if they exist:
for cmdoptions in $@; do
  if [ "$cmdoptions" = "--removemenuitem" ]; then
    EXISTINGEXEC=`find "${HOME}"/.local/share/applications/ -name "assaultcube*" | xargs`
    if [ -n "$EXISTINGEXEC" ]; then
      echo "The following menuitem(s) currently exist:"
      echo "$EXISTINGEXEC"
      read -p "Would you like them all to be deleted? (Y/N): " -r REPLY
      if [ "$REPLY" = "y" ] || [ "$REPLY" = "yes" ] || [ "$REPLY" = "Y" ] || [ "$REPLY" = "YES" ]; then
        find "${HOME}"/.local/share/applications/ -name "assaultcube*" -delete
        echo "Deleted menuitems as requested."
        exit 0
      else
        echo "The existing menuitems will remain."
        exit 0
      fi
    else
      echo "No pre-existing menuitems were found."
      exit 1
    fi
  fi
done

# A few bits of error checking, before we try doing anything else:
if [ ! -x "$CUBEDIRC"/bin_unix/"$CUBEEXEC" ]; then
  # Maybe this script in the wrong directory?
  if [ ! -d ""$CUBEDIRC"/bin_unix" ]; then
    echo "The \"bin_unix\" folder was not found."
    echo "Please run this script from inside the main AssaultCube directory."
    echo "Or, use the --basedirectory switch to state the absolute path as an argument."
    echo "Example: sh "$0" --basedirectory=/path/to/assaultcube/directory"
    exit 1
  # Or perhaps, the binary exists, but isn't executable?
  elif [ ! -x "$CUBEDIRC"/bin_unix/"$CUBEEXEC" ] && [ -e "$CUBEDIRC"/bin_unix/"$CUBEEXEC" ]; then
    echo "Insufficient permissons to run AssaultCube."
    echo "Please change (chmod) the AssaultCube "$EXECTYPE" in the bin_unix folder to be readable/executable."
    exit 1
  else
    echo "Your platform does not have a pre-compiled AssaultCube "$EXECTYPE"."
    echo "Please follow the following steps to build a native "$EXECTYPE":"
    echo "1) Ensure you have the following DEVELOPMENT libraries installed:"
    if [ "$EXECTYPE" = "server" ]; then
      echo "   SDL, zlib, libcurl"
    else
      echo "   OpenGL, SDL, SDL_image, zlib, libogg, libvorbis, OpenAL Soft, libcurl"
    fi
    echo "2) Ensure clang++ and any other required build tools are installed."
    if [ "$EXECTYPE" = "server" ]; then
      echo "3) Change directory to ./source/src/ and type \"make server_install\"."
    else
      echo "3) Change directory to ./source/src/ and type \"make install\"."
    fi
    echo "4) If the compile succeeds, return to this directory and re-run this script."
    exit 1
  fi
fi

# Some more bits of error checking before we try anything else. This test checks if
# all required libraries are installed, then warns the user accordingly.
#
# Firstly, server requirements (as they're also required for the client):
if [ -x "/sbin/ldconfig" ]; then
  if [ -z "$(/sbin/ldconfig -p | grep "libSDL-1.2")" ]; then
    echo "To run an AssaultCube server, please ensure SDL v1.2 libraries are installed."
    exit 1
  fi
  if [ -z "$(/sbin/ldconfig -p | grep "libz")" ]; then
    echo "To run an AssaultCube server, please ensure z libraries are installed."
    exit 1
  fi
  if [ -z "$(/sbin/ldconfig -p | grep "libcurl")" ]; then
    echo "To run an AssaultCube server, please ensure Curl libraries are installed."
    exit 1
  fi
  # Next, client requirements:
  if [ "$EXECTYPE" = "client" ]; then
    if [ -z "$(/sbin/ldconfig -p | grep "libX11")" ]; then
      echo "To run AssaultCube, please ensure X11 libraries are installed."
      exit 1
    fi
    if [ -z "$(/sbin/ldconfig -p | grep "libSDL_image")" ]; then
      echo "To run AssaultCube, please ensure SDL_image libraries are installed."
      exit 1
    fi
    if [ -z "$(/sbin/ldconfig -p | grep "libogg")" ]; then
      echo "To run AssaultCube, please ensure ogg libraries are installed."
      exit 1
    fi
    if [ -z "$(/sbin/ldconfig -p | grep "libvorbis")" ]; then
      echo "To run AssaultCube, please ensure vorbis libraries are installed."
      exit 1
    fi
    if [ -z "$(/sbin/ldconfig -p | grep "libopenal")" ]; then
      echo "To run AssaultCube, please ensure OpenAL-Soft libraries are installed."
      exit 1
    fi
  fi
else
  echo "NOTE: We couldn't find /sbin/ldconfig, so we're unable to test if you have all."
  echo "of the libraries required to run an AssaultCube server. We'll continue anyway."
fi

for cmdoptions in $@; do
  if [ "$cmdoptions" = "--installmenuitem" ]; then
    # Installing the .desktop item:
    mkdir -p "${HOME}"/.local/share/applications
    cat > ""${HOME}"/.local/share/applications/assaultcube_$ACVERSION.desktop" << EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=AssaultCube $ACVERSION
Keywords=assaultcube;game;fps;
GenericName=First Person Shooter Game
Categories=Game;ActionGame;Shooter;
Terminal=false
StartupNotify=false
Exec=sh -c "cd "$CUBEDIRC" && bin_unix/$CUBEEXEC $DEFCLIENT $ACARGUMENTS"
Icon=$CUBEDIRC/docs/images/icon.png
Comment=A multiplayer, first-person shooter game, based on the CUBE engine. Fast, arcade gameplay.
EOF
    chmod +x ""${HOME}"/.local/share/applications/assaultcube_"$ACVERSION".desktop"
    if [ -x "${HOME}"/.local/share/applications/assaultcube_"$ACVERSION".desktop ]; then
      echo "An AssaultCube menuitem has been successfully installed to"
      echo ""${HOME}"/.local/share/applications/assaultcube_"$ACVERSION".desktop"
      exit 0
    else
      echo "Installing an AssaultCube menuitem has failed for an unknown reason."
      exit 1
    fi
  fi
done

# Runs an AssaultCube server, or starts serverwizard to configure a launcher for it:
if [ "$EXECTYPE" = "server" ]; then
  if [ -n "$SERVERLAUNCHER" ]; then
    cd "$CUBEDIRC" && touch "$SERVERLAUNCHER".sh
    chmod 755 "$SERVERLAUNCHER".sh
    bin_unix/"$CUBEEXEC" --wizard "$SERVERLAUNCHER".sh bin_unix/"$CUBEEXEC"
  else
    cd "$CUBEDIRC" && bin_unix/"$CUBEEXEC" $DEFSERVER $ACARGUMENTS
  fi
# Runs an AssaultCube client:
elif [ "$EXECTYPE" = "client" ]; then
  cd "$CUBEDIRC" && bin_unix/"$CUBEEXEC" $DEFCLIENT $ACARGUMENTS
else
  echo "Tried launching AssaultCube, but we couldn't detect if a server or client was required."
  echo "This is most likely a bug, rather than a user error."
fi


