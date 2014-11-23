#!/bin/bash
# Adds a custom menuitem for AssaultCube to your Desktop's launcher.

# AC Version, this suffix is used to name the userdata directory and launcher.
ACVERSION="v1.2"
#
# The two aliases below are automatically chosen, as per the scripts below it.
# You can specify them here though, if you would prefer.
#
# Path to the AssaultCube directory:
CUBEDIRC=""
# Name of the AssaultCube executable:
CUBEEXEC=""

# State the path to the AssaultCube directory.
if [ "$CUBEDIRC" = "" ]; then
  if [ "$1" = "" ]; then
    CUBEDIRC="$(dirname "$(readlink -f "${0}")")"
  else
    CUBEDIRC="$(readlink -f "$*")"
  fi
fi

# What kind of binary file are we looking for?
if [ "$CUBEEXEC" = "" ]; then
  if [ -x "${CUBEDIRC}/bin_unix/native_client" ]; then
    CUBEEXEC="native_client"
  elif [ "$(uname -s)" = "Linux" ]; then
    if [ "$(uname -m)" = "x86_64" ]; then
      CUBEEXEC="linux_64_client"
    else
      CUBEEXEC="linux_client"
    fi
  else
    if [ "$(uname -m)" = "x86_64" ]; then
      CUBEEXEC="unknown_64_client"
    else
      CUBEEXEC="unknown_client"
    fi
  fi
fi

# Remove existing menuitem, if it exists:
EXISTINGEXEC=`find "${HOME}"/.local/share/applications/ -name "assaultcube*" | xargs`
if [ "$EXISTINGEXEC" != "" ]; then
  echo "The following menuitem(s) currently exist:"
  echo "$EXISTINGEXEC"
  read -p "Would you like them to be deleted? (Y/N): " -r REPLY
  if [ "$REPLY" = "y" ] || [ "$REPLY" = "yes" ] || [ "$REPLY" = "Y" ] || [ "$REPLY" = "YES" ]; then
    find "${HOME}"/.local/share/applications/ -name "assaultcube*" -delete
    echo "Deleted menuitems as requested." && echo ""
  else
    echo "The existing menuitems will remain." && echo ""
  fi
fi

echo "We'll now install a custom menuitem for launching AssaultCube (only for your"
echo "user). If you're ready to continue, press ENTER (or ctrl+c to exit)."
read -r DUMMY

echo "Would you like to run some tests to ensure that you have installed all of the"
read -p "required libraries? Without them, AssaultCube won't be able to run (Y/N): " -r REPLY
if [ "$REPLY" = "y" ] || [ "$REPLY" = "yes" ] || [ "$REPLY" = "Y" ] || [ "$REPLY" = "YES" ]; then
  if [ -x "/sbin/ldconfig" ]; then
    if [ -z "$(/sbin/ldconfig -p | grep "libX11")" ]; then
      echo "To run AssaultCube, please ensure X11 libraries are installed."
      exit 1
    fi
    if [ -z "$(/sbin/ldconfig -p | grep "libSDL-1.2")" ]; then
      echo "To run AssaultCube, please ensure SDL v1.2 libraries are installed."
      exit 1
    fi
    if [ -z "$(/sbin/ldconfig -p | grep "libSDL_image")" ]; then
      echo "To run AssaultCube, please ensure SDL_image libraries are installed."
      exit 1
    fi
    if [ -z "$(/sbin/ldconfig -p | grep "libz")" ]; then
      echo "To run AssaultCube, please ensure z libraries are installed."
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
    if [ -z "$(/sbin/ldconfig -p | grep "libcurl")" ]; then
      echo "To run AssaultCube, please ensure Curl libraries are installed."
      exit 1
    fi
  else
    echo "Couldn't find /sbin/ldconfig, so we're unable to run these tests."
    echo "We'll install the menuitem anyway..."
  fi
else
  echo "Yes sir! No tests will be done."
fi

# TODO: Add more options to the installer, if wanted.
echo "" && echo "We'll install AssaultCube's menuitem with the default options."
read -p "Unless you want to set advanced options? (Y/N): " -r REPLY
if [ "$REPLY" = "y" ] || [ "$REPLY" = "yes" ] || [ "$REPLY" = "Y" ] || [ "$REPLY" = "YES" ]; then
  echo "" && echo "AssaultCube will store userdata in ~/.assaultcube_"$ACVERSION" by default."
  read -p "Do you want to store userdata elsewhere? (Y/N) " -r REPLY
  if [ "$REPLY" = "y" ] || [ "$REPLY" = "yes" ] || [ "$REPLY" = "Y" ] || [ "$REPLY" = "YES" ]; then
    echo "Please state the absolute path of where to store AssaultCube files. Note, that"
    read -p "you should ensure this folder doesn't conflict with existing AssaultCube versions: " -r ACHOME1
    echo "Thank you. We'll store userdata in "$ACHOME1""
    mkdir -p "$ACHOME1"
    ACHOME="--home="$ACHOME1""
  else
    echo "We'll store userdata in ~/.assaultcube_"$ACVERSION" then."
    ACHOME="--home="${HOME}"/.assaultcube_"$ACVERSION""
  fi
  echo "" && echo "AssaultCube will run the \"init\" script before running graphics/sound"
  echo "subsystems by default. We normally use ./config/init.cfg"
  echo "Note that this script normally stores options, such as fullscreen, screen width/height and"
  echo "therefore makes specifying those options via this script, fairly redundant."
  read -p "Do you want to change this? (Y/N) " -r REPLY
  if [ "$REPLY" = "y" ] || [ "$REPLY" = "yes" ] || [ "$REPLY" = "Y" ] || [ "$REPLY" = "YES" ]; then
    echo "Please state the relative path of where your new init.cfg file is. Or, leave"
    read -p "this field blank to disable this option: " -r ACINIT1
    if [ "$ACINIT1" = "" ]; then
      echo "--init has been disabled."
      ACINIT=""
    else
      echo "Thank you. We'll run "$ACINIT1" as an init script."
      ACINIT="--init=\""$ACINIT1"\""
    fi
  else
    echo "We'll run ./config/init.cfg as an init script then."
    ACINIT="--init"
  fi
else
  echo "The default options will be installed only."
  ACHOME="--home="${HOME}"/.assaultcube_"$ACVERSION""
  ACINIT="--init"
fi

# Check that a binary exists and is executable for this system:
if [ -x "$CUBEDIRC"/bin_unix/"$CUBEEXEC" ]; then
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
Exec=sh -c "cd "$CUBEDIRC" && bin_unix/$CUBEEXEC $ACHOME $ACINIT"
Icon=$CUBEDIRC/docs/images/icon.png
Comment=A multiplayer, first-person shooter game, based on the CUBE engine. Fast, arcade gameplay.
EOF
  chmod +x ""${HOME}"/.local/share/applications/assaultcube_"$ACVERSION".desktop"
  if [ -x "${HOME}"/.local/share/applications/assaultcube_"$ACVERSION".desktop ]; then
    echo "" && echo "An AssaultCube menuitem has been successfully installed to"
    echo ""${HOME}"/.local/share/applications/assaultcube_"$ACVERSION".desktop"
    exit 0
  else
    echo "" && echo "For some reason, we're unable to install an AssaultCube menuitem."
    exit 1
  fi
# But, if no binary exists, is this script perhaps in the wrong directory?
elif [ -z ""$CUBEDIRC"/bin_unix" ]; then
  echo "The \"bin_unix\" folder was not found."
  echo "Please run this script inside the main AssaultCube directory."
  echo "Or, type the path to the directory as an argument when executing "$0""
  echo "Example: bash "$0" /path/to/assaultcube/directory"
  exit 1
# Or perhaps, the binary exists, but isn't executable?
elif [ -e "$CUBEDIRC"/bin_unix/"$CUBEEXEC" ]; then
  echo "Insufficient permissons to run AssaultCube."
  echo "Please change (chmod) the AssaultCube client in the bin_unix folder to be readable/executable."
  exit 1
else
  echo "Your platform does not have a pre-compiled AssaultCube client."
  echo "Please follow the following steps to build a native client:"
  echo "1) Ensure you have the following DEVELOPMENT libraries installed:"
  echo "   OpenGL, SDL, SDL_image, zlib, libogg, libvorbis, OpenAL Soft, libcurl"
  echo "2) Ensure clang++ and any other required build tools are installed."
  echo "3) Change directory to ./source/src/ and type \"make install\"."
  echo "4) If the compile succeeds, return to this directory and re-run this script." 
  exit 1
fi
