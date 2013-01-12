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
      sed -i 's/aard/makke\/aardapple_enginebox/g' $OPTARG
      sed -i 's/barrel/makke\/barrel/g' $OPTARG
      sed -i 's/barrel2/makke\/barrel_fallen/g' $OPTARG
      sed -i 's/barrel-toxic/makke\/barrel_toxic/g' $OPTARG
      sed -i 's/rattrapbarrel/makke\/barrel_newsteel/g' $OPTARG
      sed -i 's/rattrapbarrel2/makke\/barrel_newsteel_fallen/g' $OPTARG
      sed -i 's/bench/makke\/bench_seat/g' $OPTARG
      sed -i 's/bridge/makke\/platform/g' $OPTARG
      sed -i 's/bridge_shine/makke\/platform_shine/g' $OPTARG
      sed -i 's/bulb/makke\/lightbulb/g' $OPTARG
      sed -i 's/can/makke\/coke_can/g' $OPTARG
      sed -i 's/can2/makke\/coke_can_fallen/g' $OPTARG
      sed -i 's/chair1/makke\/office_chair/g' $OPTARG
      sed -i 's/coffeemug/makke\/coffee_mug/g' $OPTARG
      sed -i 's/comp_bridge/makke\/platform_bridge/g' $OPTARG
      sed -i 's/drainpipe/makke\/drainpipe/g' $OPTARG
      sed -i 's/dumpster/makke\/dumpster/g' $OPTARG
      sed -i 's/elektro/makke\/electric_meter/g' $OPTARG
      sed -i 's/europalette/makke\/pallet/g' $OPTARG
      echo -e "\a\nConversion finished!"
    else
      echo -e "\a\E[31m\033[1mERROR:\E[0m The path you've stated to the map config file is invalid."
    fi
    else
      echo -e "\a\E[31m\033[1mERROR:\E[0m Invalid OPTION."
      echo -e "Try running this script with the -c option, specifying the path to your maps config file."
  fi
exit
done

# If no options are given, this will skip the while loop and output this error.
echo -e "\a\E[31m\033[1mERROR:\E[0m No OPTION given."
echo -e "Try running this script with the -c option, specifying the path to your maps config file."
