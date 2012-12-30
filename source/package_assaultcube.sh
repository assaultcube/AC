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
NEWACVERSION=1.2.0.0_beta



# Set auto-aliases:
ACDIRFOLDERNAME=`basename $PATHTOACDIR`

# Start of the script, so remind the user of things we can't check:
echo "Did you update to head, both the packaging directory and the docs directory?"
echo " * If so, press ENTER to continue, otherwise press ctrl+c to exit."
read
echo "If required, did you patch the source with any relevant materials?"
echo " * Press ENTER to continue, otherwise press ctrl+c to exit."
read
echo "Did you strip symobls when compiling?"
echo " * If so, press ENTER to continue, otherwise press ctrl+c to exit."
read
echo "Have you checked all 4 variables in this script are set correctly?"
echo -e " * The packages created by this script will be saved to this folder:\n    $SAVETARBALLPATH"
echo -e " * The absolute-path to your AssaultCube directory is set as:\n    $PATHTOACDIR"
echo -e " * The absolute-path to your AssaultCube \"docs\" directory is set as:\n    $ABSOLUTEPATHTODOCS"
echo -e " * Your new AssaultCube version will become:\n    $NEWACVERSION\n"
echo -e "\nIf these are correct, press ENTER to continue, otherwise press ctrl+c to exit."
read

# Still at the start of the script, so find user-errors that we can check:
# Checking "$ABSOLUTEPATHTODOCS" is set correctly...
cd "$ABSOLUTEPATHTODOCS" && if [ -e ./reference.xml ]; then
  echo "Stated path to AssaultCube's documentation contains \"reference.xml\""
  echo -e "Hopefully this means the contained files exist and are good.\n"
else
  echo "\"reference.xml\" did NOT exist at the stated alias."
  echo "Open this shell file and modify the alias ABSOLUTEPATHTODOCS to fix it."
  exit
fi

# Check that linux binaries have been created today:
cd $PATHTOACDIR/bin_unix
BINARYFILES=`find ./linux_* -mtime 0 | sort | xargs`
if [ "$BINARYFILES" = "./linux_64_client ./linux_64_server ./linux_client ./linux_server" ]; then
    echo -e "All binaries have todays timestamp. Proceeding to the next step...\n"
else
    echo "Some binaries DON'T have todays timestamp. Please go compile 32/64 bit binaries."
    echo "If the binaries seem fine, check that the PATHTOACDIR variable is set correctly in this shell script."
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
    echo -e "All shadows.dat files exist. Proceeding to the next step...\n"
else
    echo "Some shadows.dat files are missing. Please run AssaultCube first and generate them."
    exit
fi

# Clean out crufty files:
cd $PATHTOACDIR/source/enet && make distclean
cd ../src && make clean
cd ../../config && rm -f init.cfg killmessages.cfg saved.cfg servers.cfg
cd .. && rm -f `(find ./demos/* -type f | grep -v "tutorial_demo.dmo")`
rm -f ./bin_unix/native_*
rm -f ./packages/maps/*.cgz
rm -f ./screenshots/*

# Copy docs over:
cd $PATHTOACDIR && cp $ABSOLUTEPATHTODOCS/* $PATHTOACDIR/docs/ -R

# Set up ./config/docs.cfg - just in-case:
xsltproc -o ./config/docs.cfg ./docs/xml/cuberef2cubescript.xslt ./docs/reference.xml

# Set up ./config/securemaps.cfg - just in-case:
echo "resetsecuremaps" > ./config/securemaps.cfg
find ./packages/maps/official/*.cgz | \
  xargs -i basename {} .cgz | \
  xargs -i echo "securemap" {} | \
  sort -u >> ./config/securemaps.cfg

# Set up ./config/releasefiles.cfg - just in-case:
# We should be forcing good standards, so don't worry about upper-case
# extensions or leaving stuff out that isn't normally in the configs.
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
cd .. && mv $ACDIRFOLDERNAME AssaultCube_v$NEWACVERSION
tar cjvf $SAVETARBALLPATH/AssaultCube_v$NEWACVERSION.tar.bz2 \
  --exclude=*.bat --exclude=*bin_win32* \
  --exclude=*xcode* \
  --exclude=*vcpp* \
  --exclude-vcs \
  AssaultCube_v$NEWACVERSION/

# Create the source tarball:
mv AssaultCube_v$NEWACVERSION/ AssaultCube_v$NEWACVERSION.source/
tar cjvf $SAVETARBALLPATH/AssaultCube_v$NEWACVERSION.source.tar.bz2 \
  --exclude-vcs \
  AssaultCube_v$NEWACVERSION.source/source/
mv AssaultCube_v$NEWACVERSION.source/ $ACDIRFOLDERNAME/

# Clean up:
rm -Rf $PATHTOACDIR/docs/*
cd $PATHTOACDIR/packages/models && rm -f `(find . -type f -name "shadows.dat")`

