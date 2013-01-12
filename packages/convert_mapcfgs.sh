#!/bin/bash
# A simple script using SED to make map configs
# compatible with the new layout.

while getopts "c:" copt; do
  if [ "$copt" = "c" ]; then
    if [ -e "$OPTARG" ]; then
      echo "Converting your map cfg file..."
      echo -e "\nConverting mapmodels..."
      sed -i 's/laptop1/jcdpc\/laptop/g' $OPTARG
      sed -i 's/rattrap\/cbbox/jcdpc\/cardboardbox/g' $OPTARG
      sed -i 's/rattrap\/hanginlamp/jcdpc\/hanginglamp/g' $OPTARG
      sed -i 's/rattrap\/ventflap/jcdpc\/ventflap/g' $OPTARG
      sed -i 's/rattrap\/milkcrate2/ratboy\/toca_milkcrate_blue/g' $OPTARG
      sed -i 's/rattrap\/milkcrate1/ratboy\/toca_milkcrate_red/g' $OPTARG
      echo -e "\a\nConversion finished!"
    else
      echo -e "\a\E[31m\033[1mERROR:\E[0m The path you've stated to the map config file is invalid."
    fi
    else
      echo -e "\a\E[31m\033[1mERROR:\E[0m Invalid OPTION"
      echo -e "Try running this script with the -c option, specifying the path to your maps config file."
  fi
done
