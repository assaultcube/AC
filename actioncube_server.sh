#!/bin/sh
# CUBE_DIR should refer to the directory in which Cube is placed.
#CUBE_DIR=~/cube
#CUBE_DIR=/usr/local/cube
CUBE_DIR=./

# SYSTEM_NAME should be set to the name of your operating system.
#SYSTEM_NAME=Linux
SYSTEM_NAME=`uname -s`

# MACHINE_NAME should be set to the name of your processor.
#MACHINE_NAME=i686
MACHINE_NAME=`uname -m`

case ${SYSTEM_NAME} in
Linux)
  SYSTEM_PREFIX=linux_
  ;;
*)
  SYSTEM_PREFIX=native_
  ;;
esac

case ${MACHINE_NAME} in
i486|i586|i686)
  MACHINE_PREFIX=
  ;;
*)
  MACHINE_PREFIX=
  ;;
esac

if test -x ${CUBE_DIR}/bin_unix/native_server
then
  SYSTEM_PREFIX=native_
  MACHINE_PREFIX=
fi

if test -x ${CUBE_DIR}/bin_unix/${MACHINE_PREFIX}${SYSTEM_PREFIX}server
then
  cd ${CUBE_DIR}
  exec ${CUBE_DIR}/bin_unix/${MACHINE_PREFIX}${SYSTEM_PREFIX}server $*
else
  echo "Your platform does not have a pre-compiled Cube server."
  echo "Please follow the following steps to build a native server:"
  echo "1) Ensure you have the SDL, SDL-image, SDL-mixer, and OpenGL libraries installed."
  echo "2) Change directory to source/src/ and type \"make install\"."
  echo "3) If the build succeeds, return to this directory and run this script again."
  exit 1
fi

