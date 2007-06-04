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
  SYSTEM_NAME=linux_
  ;;
*)
  SYSTEM_NAME=native_
  ;;
esac

case ${MACHINE_NAME} in
i486|i586|i686)
  MACHINE_NAME=
  ;;
*)
  if [ ${SYSTEM_NAME} != native_ ]
  then
    SYSTEM_NAME=native_
  fi
  MACHINE_NAME=
  ;;
esac

if [ -x ${CUBE_DIR}/bin_unix/native_server ]
then
  SYSTEM_NAME=native_
  MACHINE_NAME=
fi

if [ -x ${CUBE_DIR}/bin_unix/${MACHINE_NAME}${SYSTEM_NAME}server ]
then
  cd ${CUBE_DIR}
  exec ${CUBE_DIR}/bin_unix/${MACHINE_NAME}${SYSTEM_NAME}server $*
else
  echo "Your platform does not have a pre-compiled Cube server."
  echo "Please follow the following steps to build a native server:"
  echo "1) Ensure you have the SDL, SDL-image, SDL-mixer, and OpenGL libraries installed."
  echo "2) Change directory to source/src/ and type \"make install\"."
  echo "3) If the build succeeds, return to this directory and run this script again."
  exit 1
fi

