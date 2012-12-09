#!/bin/sh

# Before running this script, complete this checklist:
# * Download a copy of current SVN.
# * Download a copy of current SVN documentation files.
# * Set the makefile to strip symbols.
# * Patch the source code with any relevant materials.
# * Compile new 32/64 bit binaries and put them in their appropriate ./bin_unix locations.
#   (this script doesn't do that, as I compile the 32 bit on a virtual machine).
# * Delete all non-repository files/folders except any shadows.dat and the new binaries.
#   (this script doesn't do that... yet).
# * Change the below variables before executing the script:

PATHTOACDIR=~/AssaultCube/SVN_Trunk
ACDIRFOLDERNAME=SVN_Trunk
NEWACVERSION=1.2.0.0_beta
ABSOLUTEPATHTODOCS=~/AssaultCube/SVN_Website/htdocs/docs


# Change path to the AC folder:
cd $PATHTOACDIR

# Copy docs over:
cp $ABSOLUTEPATHTODOCS/* $PATHTOACDIR/docs/ -R

# Set up ./config/docs.cfg correctly:
xsltproc -o ./config/docs.cfg ./docs/xml/cuberef2cubescript.xslt ./docs/reference.xml

# Set up "releasefiles.cfg" correctly:
head -n3 ./config/releasefiles.cfg | tee ./config/releasefiles.cfg
find ./docs ./packages -type f | grep -v ".svn" >> ./config/releasefiles.cfg

# Create the linux tarball:
cd .. && mv $ACDIRFOLDERNAME AssaultCube_v$NEWACVERSION
tar cjvf assaultcube_v$NEWACVERSION.tar.bz2 --exclude=*.bat --exclude=*bin_win32* --exclude=*xcode* --exclude=*vcpp* --exclude-vcs AssaultCube_v$NEWACVERSION/

# Create the source tarball:
mv AssaultCube_v$NEWACVERSION/ AssaultCube_v$NEWACVERSION.source/
tar cjvf assaultcube_v$NEWACVERSION.source.tar.bz2 --exclude-vcs AssaultCube_v$NEWACVERSION.source/source/
mv AssaultCube_v$NEWACVERSION.source/ $ACDIRFOLDERNAME/

