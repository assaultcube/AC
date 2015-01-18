#!/bin/bash

# Before running this script, complete this checklist:
# * Download a copy of current GIT.
# * Download a copy of current GIT documentation files.
# * Patch the source code/package with any relevant materials.
# * Set the makefile to strip symbols.
# * Compile new 32/64 bit binaries and put them in their appropriate ./bin_unix locations.
#   (this isn't done automatically, as I compile the 32-bit binary on a virtual machine).
# * Delete all non-repository files/folders except any shadows.dat and the new binaries.

# ** CHANGE THE BELOW 4 VARIABLES BEFORE EXECUTING THE SCRIPT ** #

# Please ensure this path is NOT inside your "PATHTOACDIR" folder, as
# this will be the place the tarball package gets saved.
SAVETARBALLPATH=~/AssaultCube
# This will be the directory that gets packaged:
PATHTOACDIR=~/AssaultCube/GIT_AC
ABSOLUTEPATHTODOCS=~/AssaultCube/GIT_Website/htdocs/docs
NEWACVERSION=1.2.0.2

# KSH won't work, as this script uses echo throughout it, echo's options are shell-specific:
if [ -z "$(echo "$BASH")" ]; then
  echo "FAILURE: Please run this script with BASH, not SH/KSH/ZSH, etc"
  exit
fi

# Set auto-aliases:
ACDIRFOLDERNAME=`basename $PATHTOACDIR`

# Start of the script, so remind the user of things we can't check:
echo -e "\n\033[1mThe following files have changed, since your last update from GIT:\033[0m"
cd "$PATHTOACDIR" && git fetch && git diff --name-status master origin/master
cd "$ABSOLUTEPATHTODOCS" && git fetch && git diff --name-status master origin/master
echo -e "\n\033[1mThe following files are not in GIT, but exist locally:\033[0m"
cd "$PATHTOACDIR" && git ls-files --other --exclude-standard
cd "$ABSOLUTEPATHTODOCS" && git ls-files --other --exclude-standard
echo " * If you're happy with this, press ENTER to continue, otherwise press ctrl+c to exit."
read DUMMY
echo -e "\033[1mIf required, did you patch the source with any relevant materials?\033[0m"
echo " * Press ENTER to continue, otherwise press ctrl+c to exit."
read DUMMY
echo -e "\033[1mDid you strip symbols when compiling?\033[0m"
echo " * If so, press ENTER to continue, otherwise press ctrl+c to exit."
read DUMMY
echo -e "\033[1mIs the save-path in assaultcube.sh correct?\033[0m"
echo " * If so, press ENTER to continue, otherwise press ctrl+c to exit."
read DUMMY
echo -e "\033[1mHave you checked all 4 variables in this script are set correctly?\033[0m"
echo -e " * The packages created by this script will be saved to this folder:\n    $SAVETARBALLPATH"
echo -e " * The absolute-path to your AssaultCube directory is set as:\n    $PATHTOACDIR"
echo -e " * The absolute-path to your AssaultCube \"docs\" directory is set as:\n    $ABSOLUTEPATHTODOCS"
echo -e " * Your new AssaultCube version will become:\n    $NEWACVERSION\n"
echo -e "\033[1mIf these are correct, press ENTER to continue, otherwise press ctrl+c to exit.\033[0m"
read DUMMY

# Still at the start of the script, so find user-errors that we can check:
# Checking "$ABSOLUTEPATHTODOCS" is set correctly...
if [ -e $ABSOLUTEPATHTODOCS/reference.xml ]; then
  echo "Stated path to AssaultCube's documentation contains \"reference.xml\""
  echo "Hopefully this means the path is correct and contained files are good."
  echo -e "Proceeding to the next step...\n"
else
  echo -e "\033[1;31mERROR:\033[0m \"reference.xml\" did NOT exist at the ABSOLUTEPATHTODOCS alias."
  echo "Open this shell file and modify that alias to fix it."
  exit
fi

# Checking "$PATHTOACDIR" is set correctly...
if [ -e $PATHTOACDIR/source/src/main.cpp ]; then
  echo "Stated path to AssaultCube contains \"source/src/main.cpp\""
  echo "Hopefully this means the path is correct and contained files are good."
  echo -e "Proceeding to the next step...\n"
else
  echo -e "\033[1;31mERROR:\033[0m \"source/src/main.cpp\" did NOT exist at the PATHTOACDIR alias."
  echo "Open this shell file and modify that alias to fix it."
  exit
fi

# Checking for "xsltproc"...
if [ -n "$(command -v xsltproc)" ]; then
  echo "The 'xsltproc' program exists. This means we can generate docs.cfg"
  echo -e "Proceeding to the next step...\n"
else
  echo -e "\033[1;31mERROR:\033[0m 'xsltproc' doesn't exist."
  echo "Please install xsltproc so docs.cfg can be generated."
  exit
fi

# Check that linux binaries have been created today:
BINARYFILES=`cd $PATHTOACDIR/bin_unix && find ./linux_* -mtime 0 | sort | xargs`
if [ "$BINARYFILES" = "./linux_64_client ./linux_64_server ./linux_client ./linux_server" ]; then
    echo "All binaries have been compiled in the last 24 hours. Hopefully this means they're up to date!"
    echo -e "Proceeding to the next step...\n"
else
    echo -e "\033[1;31mERROR:\033[0m Timestamps on binary files are older than 24 hours."
    echo "Please go compile new 32/64 bit binaries."
    exit
fi

# Check that map previews are all available:
MAPSPATH="$PATHTOACDIR/packages/maps/official"
MAPSCGZ=`cd $MAPSPATH && find ./*.cgz | xargs -i basename {} .cgz | sort -u | xargs`
MAPSJPG=`cd $MAPSPATH/preview && find ./*.jpg | xargs -i basename {} .jpg | sort -u | xargs`
if [ "$MAPSCGZ" = "$MAPSJPG" ]; then
    echo "All map \"previews\" are available!"
    echo -e "Proceeding to the next step...\n"
else
    echo -e "\033[1;31mERROR:\033[0m Some map previews are missing."
    echo "Please go to $MAPSPATH/preview and create the ones that are missing!"
    exit
fi

# Check that bot waypoints are all available:
MAPSCGZ2=`cd $MAPSPATH && find ./*.cgz | xargs -i basename {} .cgz | sort -u | xargs`
MAPSBOTS=`cd $PATHTOACDIR/bot/waypoints && find ./*.wpt | xargs -i basename {} .wpt | sort -u | xargs`
if [ "$MAPSBOTS" = "$MAPSCGZ2" ]; then
    echo "All bot waypoints are available!"
    echo -e "Proceeding to the next step...\n"
else
    echo -e "\033[1;31mERROR:\033[0m Some bot waypoints are missing."
    echo "Please go generate the ones that are missing!"
    exit
fi

# Check that shadows.dat files exist. To grab a list of shadows.dat files:
#    cd ./packages/models && find . -name "shadows.dat" | sort
cd $PATHTOACDIR/packages/models/
if [ -e ./misc/gib02/shadows.dat ] \
  && [ -e ./misc/gib03/shadows.dat ] \
  && [ -e ./pickups/akimbo/shadows.dat ] \
  && [ -e ./pickups/ammobox/shadows.dat ] \
  && [ -e ./pickups/health/shadows.dat ] \
  && [ -e ./pickups/helmet/shadows.dat ] \
  && [ -e ./pickups/kevlar/shadows.dat ] \
  && [ -e ./pickups/nade/shadows.dat ] \
  && [ -e ./pickups/pistolclips/shadows.dat ] \
  && [ -e ./playermodels/shadows.dat ] \
  && [ -e ./weapons/assault/world/shadows.dat ] \
  && [ -e ./weapons/carbine/world/shadows.dat ] \
  && [ -e ./weapons/grenade/static/shadows.dat ] \
  && [ -e ./weapons/grenade/world/shadows.dat ] \
  && [ -e ./weapons/knife/world/shadows.dat ] \
  && [ -e ./weapons/pistol/world/shadows.dat ] \
  && [ -e ./weapons/shotgun/world/shadows.dat ] \
  && [ -e ./weapons/sniper/world/shadows.dat ] \
  && [ -e ./weapons/subgun/world/shadows.dat ]; then
    echo "All known shadows.dat files exist."
    echo -e "Proceeding to the next step...\n"
else
    echo -e "\033[1;31mERROR:\033[0m Some shadows.dat files are missing."
    echo "Please run AssaultCube to generate these files."
    exit
fi

# Clean out crufty files:
echo "Please wait: Cleaning out some crufty files..."
cd $PATHTOACDIR/source/enet && make distclean
cd ../src && make clean	
cd ../../config && rm -f init.cfg killmessages.cfg saved.cfg servers.cfg
echo -e "// This file gets executed every time you start AssaultCube. \n\n// This is where you should put any scripts you may have created for AC." > autoexec.cfg
# Leave tutorial code here, just in case of future use:
cd .. && rm -f `(find ./demos/* -type f | grep -v "tutorial_demo.dmo")`
rm -f ./bin_unix/native_*
rm -f ./packages/maps/*.cgz
rm -f ./screenshots/*
# Delete gedit backups:
find . -type f -name "*~" -delete

# Copy docs over:
echo "Copied documentation into the main folder..."
cd $PATHTOACDIR && cp $ABSOLUTEPATHTODOCS/* $PATHTOACDIR/docs/ -R

# Set up ./config/docs.cfg - just in-case:
echo "... Generating ./config/docs.cfg"
xsltproc -o ./config/docs.cfg ./docs/xml/cuberef2cubescript.xslt ./docs/reference.xml

# Set up ./config/securemaps.cfg - just in-case:
echo "... Generating ./config/securemaps.cfg"
echo "resetsecuremaps" > ./config/securemaps.cfg
find ./packages/maps/official/*.cgz | \
  xargs -i basename {} .cgz | \
  xargs -i echo -e "securemap" {} | \
  sort -u >> ./config/securemaps.cfg

# update checksum file
$PATHTOACDIR/source/dev_tools/generate_md5_checksums.sh

# Create the linux tarball:
echo -e "\n... Creating the Linux tarball..."
cd .. && mv $ACDIRFOLDERNAME AssaultCube_v$NEWACVERSION
tar cjvf $SAVETARBALLPATH/AssaultCube_v$NEWACVERSION.tar.bz2 \
  --exclude=".*" \
  --exclude=*.bat --exclude=*bin_win32* \
  --exclude=*xcode* \
  --exclude=*vcpp* \
  --exclude=*new_content.cfg \
  --exclude=*new_content.cgz \
  --exclude-vcs \
  AssaultCube_v$NEWACVERSION/

# Clean up:
cd $PATHTOACDIR && git clean -fdx && git reset --hard
