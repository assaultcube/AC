#!/bin/sh
# Checks certain parts of an AssaultCube installation
#
# Try this, if your game does not start.
#

CUBE_DIR=$(dirname "$(readlink -f "${0}")")
cd "$CUBE_DIR"

# check libs

SERVERLIBS="libz"
CLIENTLIBS="libSDL-1.2 libz libX11 libSDL_image libogg libvorbis libopenal"

if [ -x "/sbin/ldconfig" ]; then
  echo "checking libraries:"
  for libname in $SERVERLIBS; do
    if [ -z "$(/sbin/ldconfig -p | grep $libname)" ]; then
      echo " library $libname not found - this lib is required for server and client installations"
    else
      echo " found $libname"
    fi
  done
  for libname in $CLIENTLIBS; do
    if [ -z "$(/sbin/ldconfig -p | grep $libname)" ]; then
      echo " library $libname not found - this lib is required for client installations only"
    else
      echo " found $libname"
    fi
  done
else
  echo "skipping libraries check: couldn't find /sbin/ldconfig"
  echo " to run a server, you'll need:" $SERVERLIBS
  echo " to run a client, you'll need:" $SERVERLIBS $CLIENTLIBS
fi
echo ""

# check all files that belong to the installation
# (use source/dev_tools/generate_md5_checksums.sh to generate a checksum file)

CHKSUMFILE="packages/misc/checksums_md5.txt"
if [ -e $CHKSUMFILE ]; then
  echo "checking installed game files:"
  if md5sum -c $CHKSUMFILE --status; then
    echo " all files of the AC package are checked and OK"
  else
    echo " some files of your AC installation appear to be missing or damaged"
    read -p " press enter, to see the list of files..." -r dummy
    md5sum -c $CHKSUMFILE --quiet | less
  fi
else
  echo "skipping file check: couldn't find $CHKSUMFILE"
fi
echo ""

# list existing desktop launcher items

LAUNCHERPATH="${HOME}/.local/share/applications/"
EXISTINGEXEC=`find "${LAUNCHERPATH}" -name "assaultcube*" | xargs`
if [ "$EXISTINGEXEC" != "" ]; then
  echo "the following menuitem(s) wer found:"
  echo " $EXISTINGEXEC"
else
  echo "no desktop launcher items were found - run install_or_remove_menuitem.sh to create one"
fi
echo ""

# find out, which binaries would be used to start the game

echo "checking game executables:"
SERVERBIN=`./server.sh --outputbinarypath`
CLIENTBIN=`./assaultcube.sh --outputbinarypath`

if [ -x "$SERVERBIN" ]; then
  echo " the server will use the binary $SERVERBIN"
elif [ -e "$SERVERBIN" ]; then
  echo " the server would use the binary $SERVERBIN, if the permissions would allow it"
else
  echo " the server would use the binary $SERVERBIN, but the file does not exist"
fi

if [ -x "$CLIENTBIN" ]; then
  echo " the game client will use the binary $CLIENTBIN"
elif [ -e "$CLIENTBIN" ]; then
  echo " the server would use the binary $CLIENTBIN, if the permissions would allow it"
else
  echo " the server would use the binary $CLIENTBIN, but the file does not exist"
fi

exit 0

