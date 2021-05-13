#!/bin/sh
p2assets="app/src/main/assets"
# copy a selected subset of the files to the asset directory so that they are available for android
# do not commit the assets folder to git to avoid redundant files
for sub in bot config packages; do
	echo -n "$sub "
	cp -r "../../${sub}" "${p2assets}/"
done
echo
# keep these empty directories
for sub in demos; do
	if [ ! -d "${p2assets}/${sub}" ]; then
		doit="mkdir ${p2assets}/${sub}"
		#echo "$doit" 
		eval "$doit"
	fi
done
