#!/bin/sh
# CUBE_DIR should refer to the directory in which Cube is placed.
#CUBE_DIR=~/cube
#CUBE_DIR=/usr/local/lib/cube
CUBE_DIR=./

# SYSTEM_NAME should be set to the name of your operating system.
#SYSTEM_NAME=Linux
SYSTEM_NAME=`uname -s`

# MACHINE_NAME should be set to the name of your processor.
#MACHINE_NAME=i686
MACHINE_NAME=`uname -m`

case ${SYSTEM_NAME} in
Linux)
  SYSTEM_PREFIX=
  ;;
*)
  echo "Your operating system does not have a pre-compiled Cube client."
  exit 1
  ;;
esac

case ${MACHINE_NAME} in
i486|i586|i686)
  MACHINE_PREFIX=
  ;;
*)
  echo "Your processor does not have a pre-compiled Cube client."
  exit 1
  ;;
esac

cd ${CUBE_DIR}
exec ${CUBE_DIR}/bin_unix/${MACHINE_PREFIX}${SYSTEM_PREFIX}ac_client $*

