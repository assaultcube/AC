#!/bin/bash
# A simple script using SED to make map configs
# compatible with the new layout.

while getopts "c:" copt; do
  if [ "$copt" = "c" ]; then
    if [ -e "$OPTARG" ]; then
      echo "Converting your map cfg file..."
      echo -e "\nConverting mapmodels..."
      sed -i 's/\<laptop1\>/jcdpc\/laptop/g' $OPTARG
      sed -i 's/\<rattrap\/cbbox\>/jcdpc\/cardboardbox/g' $OPTARG
      sed -i 's/\<rattrap\/hanginlamp\>/jcdpc\/hanginglamp/g' $OPTARG
      sed -i 's/\<rattrap\/ventflap\>/jcdpc\/ventflap/g' $OPTARG
      sed -i 's/\<rattrap\/milkcrate2\>/ratboy\/toca_milkcrate_blue/g' $OPTARG
      sed -i 's/\<rattrap\/milkcrate1\>/ratboy\/toca_milkcrate_red/g' $OPTARG
      sed -i 's/\<aard\>/makke\/aardapple_enginebox/g' $OPTARG
      sed -i '/toxic/!s/\<barrel\>/makke\/barrel/g' $OPTARG
      sed -i 's/\<barrel2\>/makke\/barrel_fallen/g' $OPTARG
      sed -i 's/\<barrel-toxic\>/makke\/barrel_toxic/g' $OPTARG
      sed -i 's/\<rattrapbarrel\>/makke\/barrel_newsteel/g' $OPTARG
      sed -i 's/\<rattrapbarrel2\>/makke\/barrel_newsteel_fallen/g' $OPTARG
      sed -i 's/\<bench\>/makke\/bench_seat/g' $OPTARG
      sed -i 's/\<bridge\>/makke\/platform/g' $OPTARG
      sed -i 's/\<bridge_shine\>/makke\/platform_shine/g' $OPTARG
      sed -i 's/\<bulb\>/makke\/lightbulb/g' $OPTARG
      sed -i 's/\<can\>/makke\/coke_can/g' $OPTARG
      sed -i 's/\<can2\>/makke\/coke_can_fallen/g' $OPTARG
      sed -i 's/\<chair1\>/makke\/office_chair/g' $OPTARG
      sed -i 's/\<coffeemug\>/makke\/coffee_mug/g' $OPTARG
      sed -i 's/\<comp_bridge\>/makke\/platform_bridge/g' $OPTARG
      sed -i 's/\<drainpipe\>/makke\/drainpipe/g' $OPTARG
      sed -i 's/\<dumpster\>/makke\/dumpster/g' $OPTARG
      sed -i 's/\<elektro\>/makke\/electric_meter/g' $OPTARG
      sed -i 's/\<europalette\>/makke\/pallet/g' $OPTARG
      sed -i 's/\<fag\>/makke\/cigarette/g' $OPTARG
      sed -i 's/\<fence\>/makke\/fence_chainlink/g' $OPTARG
      sed -i 's/\<fencegate_closed\>/makke\/fence_chainlink_closed_gate/g' $OPTARG
      sed -i 's/\<fencegate_open\>/makke\/fence_chainlink_no_gate/g' $OPTARG
      sed -i 's/\<fencepost\>/makke\/fence_chainlink_post/g' $OPTARG
      sed -i 's/\<flyer\>/makke\/flyer_propaganda/g' $OPTARG
      sed -i 's/\<tree01\>/makke\/flyer_environmental/g' $OPTARG
      sed -i 's/\<gastank\>/makke\/fuel_tank/g' $OPTARG
      sed -i 's/\<icicle\>/makke\/icicle/g' $OPTARG
      sed -i 's/\<hook\>/makke\/hook/g' $OPTARG
      sed -i 's/\<locker\>/makke\/locker/g' $OPTARG
      sed -i 's/\<light01\>/makke\/fluorescent_lamp/g' $OPTARG
      sed -i 's/\<wood01\>/makke\/broken_wood/g' $OPTARG
      sed -i 's/\<wrench\>/makke\/wrench/g' $OPTARG
      sed -i 's/\<strahler\>/makke\/wall_spotlight/g' $OPTARG
      sed -i '/floppy/!s/\<streetlamp\>/makke\/street_light/g' $OPTARG
      sed -i 's/\<ladder_rung\>/makke\/ladder_1x/g' $OPTARG
      sed -i 's/\<ladder_7x\>/makke\/ladder_7x/g' $OPTARG
      sed -i 's/\<ladder_8x\>/makke\/ladder_8x/g' $OPTARG
      sed -i 's/\<ladder_10x\>/makke\/ladder_10x/g' $OPTARG
      sed -i 's/\<ladder_11x\>/makke\/ladder_11x/g' $OPTARG
      sed -i 's/\<ladder_15x\>/makke\/ladder_15x/g' $OPTARG
      sed -i 's/\<ladderx15_center3\>/makke\/ladder_15x_offset/g' $OPTARG
      sed -i 's/\<gutter_h\>/makke\/grate_hor/g' $OPTARG
      sed -i 's/\<gutter_v\>/makke\/grate_vert/g' $OPTARG
      sed -i 's/\<minelift\>/makke\/mine-shaft_elevator/g' $OPTARG
      sed -i 's/\<screw\>/makke\/bolt_nut/g' $OPTARG
      sed -i 's/\<sail\>/makke\/sail/g' $OPTARG
      sed -i 's/\<snowsail\>/makke\/sail_snow/g' $OPTARG
      sed -i 's/\<wires\/2x8\>/makke\/wires\/2x8/g' $OPTARG
      sed -i 's/\<wires\/3x8\>/makke\/wires\/3x8/g' $OPTARG
      sed -i 's/\<wires\/4x8\>/makke\/wires\/4x8/g' $OPTARG
      sed -i 's/\<wires\/4x8a\>/makke\/wires\/4x8a/g' $OPTARG
      sed -i 's/\<poster\>/makke\/signs\/wanted/g' $OPTARG
      sed -i 's/\<signs\/arab\>/makke\/signs\/arab/g' $OPTARG
      sed -i '/toca/!s/\<signs\/biohazard\>/makke\/signs\/biohazard/g' $OPTARG
      sed -i 's/\<signs\/caution\>/makke\/signs\/caution_voltage/g' $OPTARG
      sed -i 's/\<signs\/maint\>/makke\/signs\/caution_maintainence/g' $OPTARG
      sed -i 's/\<signs\/flammable\>/makke\/signs\/flammable/g' $OPTARG
      sed -i 's/\<signs\/speed\>/makke\/signs\/speed/g' $OPTARG
      sed -i 's/\<nocamp\>/makke\/signs\/no_camping/g' $OPTARG
      sed -i 's/\<roadblock01\>/makke\/roadblock/g' $OPTARG
      sed -i 's/\<roadblock02\>/makke\/roadblock_graffiti/g' $OPTARG
      sed -i 's/\<nothing\>/makke\/nothing_clip/g' $OPTARG
      sed -i 's/\<picture1\>/makke\/picture/g' $OPTARG
      sed -i 's/\<plant01\>/makke\/plant_leafy/g' $OPTARG
      sed -i 's/\<plant01_d\>/makke\/plant_leafy_dry/g' $OPTARG
      sed -i 's/\<plant01_s\>/makke\/plant_leafy_snow/g' $OPTARG
      sed -i 's/\<grass01\>/makke\/grass_short/g' $OPTARG
      sed -i 's/\<grass01_d\>/makke\/grass_short_dry/g' $OPTARG
      sed -i 's/\<grass01_s\>/makke\/grass_short_snow/g' $OPTARG
      sed -i 's/\<grass02\>/makke\/grass_long/g' $OPTARG
      sed -i 's/\<grass02_d\>/makke\/grass_long_dry/g' $OPTARG
      sed -i 's/\<grass02_s\>/makke\/grass_long_snow/g' $OPTARG
      echo -e "\nConverting textures..."
      sed -i 's/\<wotwot\/skin\/drainpipe.jpg\>/..\/..\/..\/models\/mapmodels\/wotwot\/makke_drainpipe_gritty\/skin.jpg/g' $OPTARG
      sed -i 's/\<wotwot\/skin\/commrack.jpg\>/..\/..\/..\/models\/mapmodels\/wotwot\/toca_commrack_dull\/skin.jpg/g' $OPTARG
      sed -i 's/\<wotwot\/skin\/monitor.jpg\>/..\/..\/..\/models\/mapmodels\/wotwot\/toca_monitor_dull\/skin.jpg/g' $OPTARG
      sed -i 's/\<wotwot\/skin\/milkcarton.jpg\>/..\/..\/..\/models\/mapmodels\/wotwot\/toca_milkcarton_dull\/skin.jpg/g' $OPTARG
      sed -i 's/\<wotwot\/skin\/guardrail2.jpg\>/..\/..\/..\/models\/mapmodels\/wotwot\/toca_guardrail2_dull\/skin.jpg/g' $OPTARG
      echo -e "\a\nConversion finished!"
      echo "Please check your map config file for any inconsistencies. Most commonly, in map comments, the word \"can\" may get accidentally converted."
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
