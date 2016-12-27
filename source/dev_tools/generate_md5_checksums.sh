#!/bin/bash
#
# update packages/misc/checksums_md5.txt
#
# (this script needs to be in source/dev_tools)

cd $(dirname "$(readlink -f "${0}")")
cd ../..    # get to the base dir of the installation

echo Generating checksums...
find [A-z]* -type f -print0 | xargs -0 md5sum | fgrep -v checksums_md5 | fgrep -v "  source/" | fgrep -v "gitignore" > packages/misc/checksums_md5.txt
echo Done.

