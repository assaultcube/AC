#!/bin/sh

# Before running this script, complete this checklist:
# * Download a copy of current SVN.
# * Download a copy of current SVN documentation files.
# * Patch the source code with any relevant materials.
# * Set the makefile to strip symbols.
# * Compile new 32/64 bit binaries and put them in their appropriate ./bin_unix locations.
#   (this isn't done automatically, as I compile the 32-bit binary on a virtual machine).
# * Delete all non-repository files/folders except any shadows.dat and the new binaries.

# ** CHANGE THE BELOW 4 VARIABLES BEFORE EXECUTING THE SCRIPT ** #

# Please ensure this path is NOT inside your "PATHTOACDIR" folder, as
# this will be the place the tarball package gets saved.
SAVETARBALLPATH=~/AssaultCube
# This will be the directory that gets packaged:
PATHTOACDIR=~/AssaultCube/SVN_Trunk
ABSOLUTEPATHTODOCS=~/AssaultCube/SVN_Website/htdocs/docs
NEWACVERSION=1.2.0.0



# Set auto-aliases:
ACDIRFOLDERNAME=`basename $PATHTOACDIR`

# Start of the script, so remind the user of things we can't check:
echo -e "\033[1mDid you update to head, both the packaging directory and the docs directory?\033[0m\a"
echo " * If so, press ENTER to continue, otherwise press ctrl+c to exit."
read
echo -e "\033[1mIf required, did you patch the source with any relevant materials?\033[0m\a"
echo " * Press ENTER to continue, otherwise press ctrl+c to exit."
read
echo -e "\033[1mDid you strip symbols when compiling?\033[0m\a"
echo " * If so, press ENTER to continue, otherwise press ctrl+c to exit."
read
echo -e "\033[1mHave you checked all 4 variables in this script are set correctly?\033[0m"
echo -e " * The packages created by this script will be saved to this folder:\n\E[31m    $SAVETARBALLPATH \E[0m"
echo -e " * The absolute-path to your AssaultCube directory is set as:\n\E[31m    $PATHTOACDIR \E[0m"
echo -e " * The absolute-path to your AssaultCube \"docs\" directory is set as:\n\E[31m    $ABSOLUTEPATHTODOCS \E[0m"
echo -e " * Your new AssaultCube version will become:\n\E[31m    $NEWACVERSION \E[0m\n"
echo -e "\033[1mIf these are correct, press ENTER to continue, otherwise press ctrl+c to exit.\033[0m\a"
read

# Still at the start of the script, so find user-errors that we can check:
# Checking "$ABSOLUTEPATHTODOCS" is set correctly...
if [ -e $ABSOLUTEPATHTODOCS/reference.xml ]; then
  echo "Stated path to AssaultCube's documentation contains \"reference.xml\""
  echo "Hopefully this means the path is correct and contained files are good."
  echo -e "Proceeding to the next step...\n"
else
  echo -e "\a\E[31m\033[1mERROR:\E[0m \"reference.xml\" did NOT exist at the ABSOLUTEPATHTODOCS alias."
  echo "Open this shell file and modify that alias to fix it."
  exit
fi

# Checking "$PATHTOACDIR" is set correctly...
if [ -e $PATHTOACDIR/source/src/main.cpp ]; then
  echo "Stated path to AssaultCube contains \"source/src/main.cpp\""
  echo "Hopefully this means the path is correct and contained files are good."
  echo -e "Proceeding to the next step...\n"
else
  echo -e "\a\E[31m\033[1mERROR:\E[0m \"source/src/main.cpp\" did NOT exist at the PATHTOACDIR alias."
  echo "Open this shell file and modify that alias to fix it."
  exit
fi

# Check that linux binaries have been created today:
BINARYFILES=`cd $PATHTOACDIR/bin_unix && find ./linux_* -mtime 0 | sort | xargs`
if [ "$BINARYFILES" = "./linux_64_client ./linux_64_server ./linux_client ./linux_server" ]; then
    echo "All binaries have been compiled in the last 24 hours. Hopefully this means they're up to date!"
    echo -e "Proceeding to the next step...\n"
else
    echo -e "\a\E[31m\033[1mERROR:\E[0m Timestamps on binary files are older than 24 hours."
    echo "Please go compile new 32/64 bit binaries."
    exit
fi

# Check that shadows.dat files exist. To grab a list of shadows.dat files:
#    cd ./packages/models && find . -name "shadows.dat" | sort
cd $PATHTOACDIR/packages/models/
if [[ -e ./misc/gib01/shadows.dat \
  && -e ./misc/gib02/shadows.dat \
  && -e ./misc/gib03/shadows.dat \
  && -e ./pickups/akimbo/shadows.dat \
  && -e ./pickups/ammobox/shadows.dat \
  && -e ./pickups/health/shadows.dat \
  && -e ./pickups/helmet/shadows.dat \
  && -e ./pickups/kevlar/shadows.dat \
  && -e ./pickups/nade/shadows.dat \
  && -e ./pickups/pistolclips/shadows.dat \
  && -e ./playermodels/shadows.dat \
  && -e ./weapons/assault/world/shadows.dat \
  && -e ./weapons/carbine/world/shadows.dat \
  && -e ./weapons/grenade/world/shadows.dat \
  && -e ./weapons/grenade/static/shadows.dat \
  && -e ./weapons/knife/world/shadows.dat \
  && -e ./weapons/pistol/world/shadows.dat \
  && -e ./weapons/shotgun/world/shadows.dat \
  && -e ./weapons/sniper/world/shadows.dat \
  && -e ./weapons/subgun/world/shadows.dat ]]; then
    echo "All known shadows.dat files exist."
    echo -e "Proceeding to the next step...\n"
else
    echo -e "\a\E[31m\033[1mERROR:\E[0m Some shadows.dat files are missing."
    echo "Please run AssaultCube to generate these files."
    exit
fi

# Clean out crufty files:
echo "Please wait: Cleaning out some crufty files..."
cd $PATHTOACDIR/source/enet && make distclean
cd ../src && make clean
cd ../../config && rm -f init.cfg killmessages.cfg saved.cfg servers.cfg
cd .. && rm -f `(find ./demos/* -type f | grep -v "tutorial_demo.dmo")`
rm -f ./bin_unix/native_*
rm -f ./packages/maps/*.cgz
rm -f ./screenshots/*

# Copy docs over:
echo "Copied documentation into the main folder..."
cd $PATHTOACDIR && cp $ABSOLUTEPATHTODOCS/* $PATHTOACDIR/docs/ -R

# Show us modified files:
echo -e "\nPlease wait: Checking if any \"extra\" crufty files exist..."
cd $PATHTOACDIR && svn status
echo -e "\n\033[1mThe above is a list of modified/extra files/folders.\033[0m\a"
echo " * If everything is OK, press ENTER to continue. Otherwise press ctrl+c to exit."
read

# Set up ./config/docs.cfg - just in-case:
echo "... Generating ./config/docs.cfg"
xsltproc -o ./config/docs.cfg ./docs/xml/cuberef2cubescript.xslt ./docs/reference.xml

# Set up ./config/securemaps.cfg - just in-case:
echo "... Generating ./config/securemaps.cfg"
echo "resetsecuremaps" > ./config/securemaps.cfg
find ./packages/maps/official/*.cgz | \
  xargs -i basename {} .cgz | \
  xargs -i echo "securemap" {} | \
  sort -u >> ./config/securemaps.cfg

# Set up ./config/releasefiles.cfg - just in-case:
# We should be forcing good standards, so don't worry about upper-case
# extensions or leaving stuff out that isn't normally in the configs.
echo "... Generating ./config/releasefiles.cfg"
find ./packages/audio/ambience ./packages/textures -type f \
  -name "*.jpg" \
  -o -name "*.png" \
  -o -name "*.ogg" | \
  grep -vE '(.svn|\/textures\/skymaps\/)' | \
  gawk -F'\\./' '{print $2}' > ./config/releasefiles.cfg
find ./packages/models/mapmodels/* -type d | \
  grep -v ".svn" | \
  gawk -F'\\./' '{print $2}' >> ./config/releasefiles.cfg
find ./packages/textures/skymaps/* -type f -name "*.jpg" | \
  grep -v ".svn" | \
  gawk -F'_' '{print $1}' | \
  sort -u | \
  gawk -F'\\./' '{print $2}' >> ./config/releasefiles.cfg

# Create the linux tarball:
echo -e "\n... Creating the Linux tarball..."
cd .. && mv $ACDIRFOLDERNAME AssaultCube_v$NEWACVERSION
tar cjvf $SAVETARBALLPATH/AssaultCube_v$NEWACVERSION.tar.bz2 \
  --exclude=*.bat --exclude=*bin_win32* \
  --exclude=*xcode* \
  --exclude=*vcpp* \
  --exclude-vcs \
  AssaultCube_v$NEWACVERSION/

# Create the source tarball:
echo -e "\n... Creating the Source tarball..."
mv AssaultCube_v$NEWACVERSION/ AssaultCube_v$NEWACVERSION.source/
tar cjvf $SAVETARBALLPATH/AssaultCube_v$NEWACVERSION.source.tar.bz2 \
  --exclude-vcs \
  AssaultCube_v$NEWACVERSION.source/source/
mv AssaultCube_v$NEWACVERSION.source/ $ACDIRFOLDERNAME/

# Clean up:
rm -Rf $PATHTOACDIR/docs/* && echo -e "\n... Deleted all files in ./docs..."
cd $PATHTOACDIR/packages/models && rm -f `(find . -type f -name "shadows.dat")`
echo "... Deleted shadows.dat files. Packaging finished!"

