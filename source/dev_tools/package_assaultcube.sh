#!/bin/bash

# Before creating compressed package complete this checklist (main points):
# * Patch the source code/package with any relevant materials.
# * Set the makefile to strip symbols.
# * Compile new 32/64-bit binaries and put them in their appropriate ./bin_unix location.
# * Delete all unnecessary files/folders.

# ** CHANGE THE BELOW VARIABLES BEFORE EXECUTING THE SCRIPT ** #

#PATHTOACDEV=$(dirname $(cd ../ && pwd))
# Please ensure this path is NOT inside your "ACDIR" folder, as
# this will be the place the tarball package gets saved.
SAVETARBALLPATH=~/AssaultCube
# This will be the directory that gets packaged:
ACDIR=$SAVETARBALLPATH/AC
ACDIRTESTING=$SAVETARBALLPATH/AC_testing
ACDOCSDIR=$ACDIR/docs
NEWACTAG=$(git describe --tags --abbrev=0)
NEWACVERSION=$(echo $NEWACTAG |awk '{ print substr($0,2)}')

# KSH won't work, as this script uses echo throughout it, echo's options are shell-specific:
if [ -z "$BASH" ]; then
  echo "FAILURE: Please run this script with BASH (bash $0), not SH/KSH/ZSH, etc."
  exit
fi

# Set auto-aliases:
ACDIRFOLDERNAME=`basename $ACDIR`

# Create folder for new AC release:
if [ ! -d "$ACDIR" ]; then
  mkdir -p "$ACDIR"
else
  rm -r "$ACDIR"
  mkdir -p "$ACDIR"
fi

# Create folder for compiling binaries for new AC release:
if [ ! -d "$ACDIRTESTING" ]; then
  mkdir "$ACDIRTESTING"
else
  rm -r "$ACDIRTESTING"
  mkdir "$ACDIRTESTING"
fi

# Get AC files from latest local release tag:
cd ../../
git archive --format=tar.gz --output "$ACDIR/$NEWACTAG.tar.gz" $NEWACTAG
cd "$ACDIR"
tar -zxf "$ACDIR/$NEWACTAG.tar.gz"
tar -zxf "$ACDIR/$NEWACTAG.tar.gz" -C "$ACDIRTESTING"
rm -r "$ACDIR/$NEWACTAG.tar.gz"

# Use new, "portable" profile in "$ACDIRTESTING":
sed -i 's/--home=.*\s/--home=profile /' $ACDIRTESTING/assaultcube.sh

# Get documentation files from latest remote release tag:
svn export --force https://github.com/assaultcube/assaultcube.github.io/tags/$NEWACTAG/htdocs/docs/ $ACDOCSDIR

# Start of the script, so remind the user of things we can't check:
echo -e "\033[1mIf required, did you patch the source with any relevant materials?\033[0m"
echo " * Press ENTER to continue, otherwise press ctrl+c to exit."
read DUMMY
echo -e "\033[1mDid you compile 32- and 64-bit binaries in $ACDIRTESTING and strip symbols when compiling?\033[0m"
echo " * If so, press ENTER to continue, otherwise press ctrl+c to exit."
read DUMMY
echo -e "\033[1mDid you renamed filenames of created binaries to linux_* and linux_64_*?\033[0m"
echo " * If so, press ENTER to continue, otherwise press ctrl+c to exit."
read DUMMY
echo -e "\033[1mDid you generate fresh config/mapmodelattributes.cfg (/loadallmapmodels)?\033[0m"
echo " * If so, press ENTER to continue, otherwise press ctrl+c to exit."
read DUMMY
echo -e "\033[1mIs the save-path in assaultcube.sh correct?\033[0m"
echo " * If so, press ENTER to continue, otherwise press ctrl+c to exit."
read DUMMY
echo -e "\033[1mHave you checked all following variables in this script are set correctly?\033[0m"
echo -e " * The packages created by this script will be saved to this folder:\n    $SAVETARBALLPATH"
echo -e " * The absolute-path to your AssaultCube directory is set as:\n    $ACDIR"
echo -e " * The absolute-path to your AssaultCube \"docs\" directory is set as:\n    $ACDOCSDIR"
echo -e " * Your new AssaultCube version will become:\n    $NEWACVERSION\n"
echo -e "\033[1mIf these are correct, press ENTER to continue, otherwise press ctrl+c to exit.\033[0m"
read DUMMY

# Still at the start of the script, so find user-errors that we can check:
# Checking "$ACDIR" is set correctly:
if [ -e $ACDIR/source/src/main.cpp ]; then
  echo "Stated path to AssaultCube contains \"source/src/main.cpp\""
  echo "Hopefully this means the path is correct and contained files are good."
  echo -e "Proceeding to the next step...\n"
else
  echo -e "\033[1;31mERROR:\033[0m \"source/src/main.cpp\" did NOT exist at the ACDIR alias."
  echo "Open this shell file and modify that alias to fix it."
  exit
fi

# Checking "$ACDOCSDIR" is set correctly:
if [ -e $ACDOCSDIR/reference.xml ]; then
  echo "Stated path to AssaultCube's documentation contains \"reference.xml\""
  echo "Hopefully this means the path is correct and contained files are good."
  echo -e "Proceeding to the next step...\n"
else
  echo -e "\033[1;31mERROR:\033[0m \"reference.xml\" does NOT exist in %ACDOCSDIR."
  echo "Copy documentation to that folder to fix it."
  exit
fi

# Check that linux binaries have been created today:
cp -v -i $ACDIRTESTING/bin_unix/linux* $ACDIR/bin_unix
checkbinaries() {
    BINARYFILES=`cd $ACDIR/bin_unix && find ./linux_* -mtime 0 | sort | xargs`
}
checkbinaries
until [ "$BINARYFILES" = "./linux_64_client ./linux_64_server ./linux_client ./linux_server" ]; do
    echo -e "\033[1;31mERROR:\033[0m Timestamps on binary files are older than 24 hours or they're missing."
    echo "Please add new 32/64-bit binaries."
    read DUMMY
    checkbinaries
done
echo "All binaries have been compiled in the last 24 hours. Hopefully this means they're up to date!"
echo -e "Proceeding to the next step...\n"

# Check that config/mapmodelattributes.cfg has been created today:
cp -v -i $ACDIRTESTING/profile/config/mapmodelattributes.cfg $ACDIR/config
checkmapmodelsattr() {
    MAPMODELATTRFILE=`cd $ACDIR/config && find ./mapmodelattributes.cfg -mtime 0 | xargs`
}
checkmapmodelsattr
until [ "$MAPMODELATTRFILE" = "./mapmodelattributes.cfg" ]; do
    echo -e "\033[1;31mERROR:\033[0m Timestamp on config/mapmodelattributes.cfg is older than 24 hours or it's missing."
    echo "Please add fresh config/mapmodelattributes.cfg file."
    read DUMMY
    checkmapmodelsattr
done
echo "config/mapmodelattributes.cfg has been generated in the last 24 hours. Hopefully this means it's up to date!"
echo -e "Proceeding to the next step...\n"

# Check that shadows.dat files exist. To grab a list of shadows.dat files:
#    cd ./packages/models && find . -name "shadows.dat" | sort
echo -e "\033[1mDid you copy shadow files (*.dat) from another working AC installation into $ACDIR?\033[0m"
echo -e " * If not, they will be automatically copied from $ACDIRTESTING"
echo " * Press ENTER to continue, otherwise press ctrl+c to exit."
read DUMMY
cd $ACDIR/packages/models/
ACDIRMODELS_SHADOWS=$ACDIRTESTING/profile/packages/models
if [ ! -e ./misc/gib01/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/misc/gib01/shadows.dat $(pwd)/misc/gib01/shadows.dat; fi
if [ ! -e ./misc/gib02/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/misc/gib02/shadows.dat $(pwd)/misc/gib02/shadows.dat; fi
if [ ! -e ./misc/gib03/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/misc/gib03/shadows.dat $(pwd)/misc/gib03/shadows.dat; fi
if [ ! -e ./pickups/akimbo/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/pickups/akimbo/shadows.dat $(pwd)/pickups/akimbo/shadows.dat; fi
if [ ! -e ./pickups/ammobox/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/pickups/ammobox/shadows.dat $(pwd)/pickups/ammobox/shadows.dat; fi
if [ ! -e ./pickups/health/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/pickups/health/shadows.dat $(pwd)/pickups/health/shadows.dat; fi
if [ ! -e ./pickups/helmet/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/pickups/helmet/shadows.dat $(pwd)/pickups/helmet/shadows.dat; fi
if [ ! -e ./pickups/kevlar/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/pickups/kevlar/shadows.dat $(pwd)/pickups/kevlar/shadows.dat; fi
if [ ! -e ./pickups/nade/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/pickups/nade/shadows.dat $(pwd)/pickups/nade/shadows.dat; fi
if [ ! -e ./pickups/pistolclips/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/pickups/pistolclips/shadows.dat $(pwd)/pickups/pistolclips/shadows.dat; fi
if [ ! -e ./playermodels/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/playermodels/shadows.dat $(pwd)/playermodels/shadows.dat; fi
if [ ! -e ./weapons/assault/world/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/weapons/assault/world/shadows.dat $(pwd)/weapons/assault/world/shadows.dat; fi
if [ ! -e ./weapons/carbine/world/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/weapons/carbine/world/shadows.dat $(pwd)/weapons/carbine/world/shadows.dat; fi
if [ ! -e ./weapons/grenade/static/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/weapons/grenade/static/shadows.dat $(pwd)/weapons/grenade/static/shadows.dat; fi
if [ ! -e ./weapons/grenade/world/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/weapons/grenade/world/shadows.dat $(pwd)/weapons/grenade/world/shadows.dat; fi
if [ ! -e ./weapons/knife/world/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/weapons/knife/world/shadows.dat $(pwd)/weapons/knife/world/shadows.dat; fi
if [ ! -e ./weapons/pistol/world/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/weapons/pistol/world/shadows.dat $(pwd)/weapons/pistol/world/shadows.dat; fi
if [ ! -e ./weapons/shotgun/world/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/weapons/shotgun/world/shadows.dat $(pwd)/weapons/shotgun/world/shadows.dat; fi
if [ ! -e ./weapons/sniper/world/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/weapons/sniper/world/shadows.dat $(pwd)/weapons/sniper/world/shadows.dat; fi
if [ ! -e ./weapons/subgun/world/shadows.dat ]; then cp -v $ACDIRMODELS_SHADOWS/weapons/subgun/world/shadows.dat $(pwd)/weapons/subgun/world/shadows.dat; fi

until [ -f ./misc/gib01/shadows.dat ] \
  && [ -f ./misc/gib02/shadows.dat ] \
  && [ -f ./misc/gib03/shadows.dat ] \
  && [ -f ./pickups/akimbo/shadows.dat ] \
  && [ -f ./pickups/ammobox/shadows.dat ] \
  && [ -f ./pickups/health/shadows.dat ] \
  && [ -f ./pickups/helmet/shadows.dat ] \
  && [ -f ./pickups/kevlar/shadows.dat ] \
  && [ -f ./pickups/nade/shadows.dat ] \
  && [ -f ./pickups/pistolclips/shadows.dat ] \
  && [ -f ./playermodels/shadows.dat ] \
  && [ -f ./weapons/assault/world/shadows.dat ] \
  && [ -f ./weapons/carbine/world/shadows.dat ] \
  && [ -f ./weapons/grenade/static/shadows.dat ] \
  && [ -f ./weapons/grenade/world/shadows.dat ] \
  && [ -f ./weapons/knife/world/shadows.dat ] \
  && [ -f ./weapons/pistol/world/shadows.dat ] \
  && [ -f ./weapons/shotgun/world/shadows.dat ] \
  && [ -f ./weapons/sniper/world/shadows.dat ] \
  && [ -f ./weapons/subgun/world/shadows.dat ]; do
    echo -e "\033[1;31mERROR:\033[0m Some shadows.dat files are missing."
    echo "Please run AssaultCube to generate these files."
    read DUMMY
done
echo "All known shadows.dat files exist."
echo -e "Proceeding to the next step...\n"

# Check that map previews are all available:
MAPSPATH="$ACDIR/packages/maps/official"
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
MAPSBOTS=`cd $ACDIR/bot/waypoints && find ./*.wpt | xargs -i basename {} .wpt | sort -u | xargs`
if [ "$MAPSBOTS" = "$MAPSCGZ2" ]; then
    echo "All bot waypoints are available!"
    echo -e "Proceeding to the next step...\n"
else
    echo -e "\033[1;31mERROR:\033[0m Some bot waypoints are missing."
    echo "Please go generate the ones that are missing!"
    exit
fi

# Delete stuff related to git and GitHub:
rm -rf ./.git
rm -f ./.gitattributes
rm -f ./.travis.yml
find . -type f -name ".gitignore" -delete
# Delete gedit backups:
find . -type f -name "*~" -delete

# Create config template for release:
cd $ACDIR/config
zip configtemplates.zip autoexec.cfg favourites.cfg pcksources.cfg

# Clean out crufty files:
echo "Please wait: Cleaning out some crufty files..."
cd $ACDIR/source/enet && make distclean
cd ../src && make clean
rm -f init*.cfg saved*.cfg servervita*.cfg servers.cfg history
rm -f ./clientlog*.txt
rm -f ./bin_unix/native_*
rm -f ./bin_unix/ac_*
rm -f ./packages/maps/dev_*.cgz
rm -f ./packages/maps/dev_*.cfg
rm -f ./packages/maps/servermaps/incoming/*.cgz
rm -f ./packages/maps/servermaps/incoming/*.cfg
rm -f ./screenshots/*
# Leave tutorial code here, just in case of future use:
cd $ACDIR && rm -f `(find ./demos/* -type f | grep -v "tutorial_demo.dmo")`

# Set up ./config/securemaps.cfg - just in-case:
echo "... Generating ./config/securemaps.cfg"
echo "resetsecuremaps" > ./config/securemaps.cfg
find ./packages/maps/official/*.cgz | \
  xargs -i basename {} .cgz | \
  xargs -i echo -e "securemap" {} | \
  sort -u >> ./config/securemaps.cfg

# update checksum file:
sh $ACDIR/source/dev_tools/generate_md5_checksums.sh

# Create the linux tarball:
echo "\n... Creating the Linux tarball..."
cd $SAVETARBALLPATH
if [ -d "AssaultCube_v$NEWACVERSION" ]; then
  rm -r "AssaultCube_v$NEWACVERSION"
fi
mv $ACDIRFOLDERNAME AssaultCube_v$NEWACVERSION
tar cjvf $SAVETARBALLPATH/AssaultCube_v$NEWACVERSION.tar.bz2 \
  --exclude=".*" \
  --exclude=*.bat \
  --exclude=*bin_win32* \
  --exclude=*doxygen* \
  --exclude="*source/lib*" \
  --exclude=*vcpp* \
  --exclude=*xcode* \
  --exclude=*AssaultCube.cbp \
  --exclude=*autoexec.cfg \
  --exclude=*favourites.cfg \
  --exclude=*pcksources.cfg \
  --exclude=*new_content.cfg \
  --exclude=*new_content.cgz \
  --exclude-vcs \
  AssaultCube_v$NEWACVERSION/
