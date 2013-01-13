#!/bin/bash
# A simple script using SED to make map configs
# compatible with the new layout.

if [[ -r "$1" && -w "$1" ]]; then
  echo "Making a backup file of $1"
  cp "$1" "$1".BAK
  if [ -r "$1".BAK ]; then
    echo "Backup successful, continuing..."
  else
    echo "Backup failed, please check files can be written in the \"`dirname $1`\" directory."
    exit
  fi

  echo "Converting $1 to a compatible map config file..."
  echo -e "\nConverting mapmodels..."
  sed -i '/^mapmodel/s/\<laptop1\>/jcdpc\/laptop/g' $1
  sed -i '/^mapmodel/s/\<rattrap\/cbbox\>/jcdpc\/cardboardbox/g' $1
  sed -i '/^mapmodel/s/\<rattrap\/hanginlamp\>/jcdpc\/hanginglamp/g' $1
  sed -i '/^mapmodel/s/\<rattrap\/ventflap\>/jcdpc\/ventflap/g' $1
  sed -i '/^mapmodel/s/\<rattrap\/milkcrate2\>/ratboy\/toca_milkcrate_blue/g' $1
  sed -i '/^mapmodel/s/\<rattrap\/milkcrate1\>/ratboy\/toca_milkcrate_red/g' $1
  sed -i '/^mapmodel/s/\<aard\>/makke\/aardapple_enginebox/g' $1
  sed -i '/toxic/!s/\<barrel\>/makke\/barrel/g' $1
  sed -i '/^mapmodel/s/\<barrel2\>/makke\/barrel_fallen/g' $1
  sed -i '/^mapmodel/s/\<barrel-toxic\>/makke\/barrel_toxic/g' $1
  sed -i '/^mapmodel/s/\<rattrapbarrel\>/makke\/barrel_newsteel/g' $1
  sed -i '/^mapmodel/s/\<rattrapbarrel2\>/makke\/barrel_newsteel_fallen/g' $1
  sed -i '/^mapmodel/s/\<bench\>/makke\/bench_seat/g' $1
  sed -i '/^mapmodel/s/\<bridge\>/makke\/platform/g' $1
  sed -i '/^mapmodel/s/\<bridge_shine\>/makke\/platform_shine/g' $1
  sed -i '/^mapmodel/s/\<bulb\>/makke\/lightbulb/g' $1
  sed -i '/^mapmodel/s/\<can\>/makke\/coke_can/g' $1
  sed -i '/^mapmodel/s/\<can2\>/makke\/coke_can_fallen/g' $1
  sed -i '/^mapmodel/s/\<chair1\>/makke\/office_chair/g' $1
  sed -i '/^mapmodel/s/\<coffeemug\>/makke\/coffee_mug/g' $1
  sed -i '/^mapmodel/s/\<comp_bridge\>/makke\/platform_bridge/g' $1
  sed -i '/^mapmodel/s/\<drainpipe\>/makke\/drainpipe/g' $1
  sed -i '/^mapmodel/s/\<dumpster\>/makke\/dumpster/g' $1
  sed -i '/^mapmodel/s/\<elektro\>/makke\/electric_meter/g' $1
  sed -i '/^mapmodel/s/\<europalette\>/makke\/pallet/g' $1
  sed -i '/^mapmodel/s/\<fag\>/makke\/cigarette/g' $1
  sed -i '/^mapmodel/s/\<fence\>/makke\/fence_chainlink/g' $1
  sed -i '/^mapmodel/s/\<fencegate_closed\>/makke\/fence_chainlink_closed_gate/g' $1
  sed -i '/^mapmodel/s/\<fencegate_open\>/makke\/fence_chainlink_no_gate/g' $1
  sed -i '/^mapmodel/s/\<fencepost\>/makke\/fence_chainlink_post/g' $1
  sed -i '/^mapmodel/s/\<flyer\>/makke\/flyer_propaganda/g' $1
  sed -i '/^mapmodel/s/\<tree01\>/makke\/flyer_environmental/g' $1
  sed -i '/^mapmodel/s/\<gastank\>/makke\/fuel_tank/g' $1
  sed -i '/^mapmodel/s/\<icicle\>/makke\/icicle/g' $1
  sed -i '/^mapmodel/s/\<hook\>/makke\/hook/g' $1
  sed -i '/^mapmodel/s/\<locker\>/makke\/locker/g' $1
  sed -i '/^mapmodel/s/\<light01\>/makke\/fluorescent_lamp/g' $1
  sed -i '/^mapmodel/s/\<wood01\>/makke\/broken_wood/g' $1
  sed -i '/^mapmodel/s/\<wrench\>/makke\/wrench/g' $1
  sed -i '/^mapmodel/s/\<strahler\>/makke\/wall_spotlight/g' $1
  sed -i '/floppy/!s/\<streetlamp\>/makke\/street_light/g' $1
  sed -i '/^mapmodel/s/\<ladder_rung\>/makke\/ladder_1x/g' $1
  sed -i '/^mapmodel/s/\<ladder_7x\>/makke\/ladder_7x/g' $1
  sed -i '/^mapmodel/s/\<ladder_8x\>/makke\/ladder_8x/g' $1
  sed -i '/^mapmodel/s/\<ladder_10x\>/makke\/ladder_10x/g' $1
  sed -i '/^mapmodel/s/\<ladder_11x\>/makke\/ladder_11x/g' $1
  sed -i '/^mapmodel/s/\<ladder_15x\>/makke\/ladder_15x/g' $1
  sed -i '/^mapmodel/s/\<ladderx15_center3\>/makke\/ladder_15x_offset/g' $1
  sed -i '/^mapmodel/s/\<gutter_h\>/makke\/grate_hor/g' $1
  sed -i '/^mapmodel/s/\<gutter_v\>/makke\/grate_vert/g' $1
  sed -i '/^mapmodel/s/\<minelift\>/makke\/mine-shaft_elevator/g' $1
  sed -i '/^mapmodel/s/\<screw\>/makke\/bolt_nut/g' $1
  sed -i '/^mapmodel/s/\<sail\>/makke\/sail/g' $1
  sed -i '/^mapmodel/s/\<snowsail\>/makke\/sail_snow/g' $1
  sed -i '/^mapmodel/s/\<wires\/2x8\>/makke\/wires\/2x8/g' $1
  sed -i '/^mapmodel/s/\<wires\/3x8\>/makke\/wires\/3x8/g' $1
  sed -i '/^mapmodel/s/\<wires\/4x8\>/makke\/wires\/4x8/g' $1
  sed -i '/^mapmodel/s/\<wires\/4x8a\>/makke\/wires\/4x8a/g' $1
  sed -i '/^mapmodel/s/\<poster\>/makke\/signs\/wanted/g' $1
  sed -i '/^mapmodel/s/\<signs\/arab\>/makke\/signs\/arab/g' $1
  sed -i '/toca/!s/\<signs\/biohazard\>/makke\/signs\/biohazard/g' $1
  sed -i '/^mapmodel/s/\<signs\/caution\>/makke\/signs\/caution_voltage/g' $1
  sed -i '/^mapmodel/s/\<signs\/maint\>/makke\/signs\/caution_maintainence/g' $1
  sed -i '/^mapmodel/s/\<signs\/flammable\>/makke\/signs\/flammable/g' $1
  sed -i '/^mapmodel/s/\<signs\/speed\>/makke\/signs\/speed/g' $1
  sed -i '/^mapmodel/s/\<nocamp\>/makke\/signs\/no_camping/g' $1
  sed -i '/^mapmodel/s/\<roadblock01\>/makke\/roadblock/g' $1
  sed -i '/^mapmodel/s/\<roadblock02\>/makke\/roadblock_graffiti/g' $1
  sed -i '/^mapmodel/s/\<nothing\>/makke\/nothing_clip/g' $1
  sed -i '/^mapmodel/s/\<picture1\>/makke\/picture/g' $1
  sed -i '/^mapmodel/s/\<plant01\>/makke\/plant_leafy/g' $1
  sed -i '/^mapmodel/s/\<plant01_d\>/makke\/plant_leafy_dry/g' $1
  sed -i '/^mapmodel/s/\<plant01_s\>/makke\/plant_leafy_snow/g' $1
  sed -i '/^mapmodel/s/\<grass01\>/makke\/grass_short/g' $1
  sed -i '/^mapmodel/s/\<grass01_d\>/makke\/grass_short_dry/g' $1
  sed -i '/^mapmodel/s/\<grass01_s\>/makke\/grass_short_snow/g' $1
  sed -i '/^mapmodel/s/\<grass02\>/makke\/grass_long/g' $1
  sed -i '/^mapmodel/s/\<grass02_d\>/makke\/grass_long_dry/g' $1
  sed -i '/^mapmodel/s/\<grass02_s\>/makke\/grass_long_snow/g' $1

  echo -e "\nConverting textures..."
  sed -i '/^texture/s/\<wotwot\/skin\/drainpipe.jpg\>/..\/models\/mapmodels\/wotwot\/makke_drainpipe_gritty\/skin.jpg/g' $1
  sed -i '/^texture/s/\<wotwot\/skin\/commrack.jpg\>/..\/models\/mapmodels\/wotwot\/toca_commrack_dull\/skin.jpg/g' $1
  sed -i '/^texture/s/\<wotwot\/skin\/monitor.jpg\>/..\/models\/mapmodels\/wotwot\/toca_monitor_dull\/skin.jpg/g' $1
  sed -i '/^texture/s/\<wotwot\/skin\/milkcarton.jpg\>/..\/models\/mapmodels\/wotwot\/toca_milkcarton_dull\/skin.jpg/g' $1
  sed -i '/^texture/s/\<wotwot\/skin\/guardrail2.jpg\>/..\/models\/mapmodels\/wotwot\/toca_guardrail2_dull\/skin.jpg/g' $1
  sed -i '/^texture/s/\<mitaman\/zastrow\/metal_overlaps.jpg\>/zastrow\/metal_overlaps.jpg/g' $1
  sed -i '/^texture/s/\<mitaman\/zastrow\/metal_plate_fill.jpg\>/zastrow\/metal_plate_fill.jpg/g' $1
  sed -i '/^texture/s/\<mitaman\/zastrow\/metal_siding_kinksb.jpg\>/zastrow\/metal_siding_kinksb.jpg/g' $1
  sed -i '/^texture/s/\<mitaman\/zastrow\/metal_siding_kinks.jpg\>/zastrow\/metal_siding_kinks.jpg/g' $1
  sed -i '/^texture/s/\<mitaman\/zastrow\/sub_doors512A10.jpg\>/zastrow\/sub_doors512A10.jpg/g' $1
  sed -i '/^texture/s/\<mitaman\/zastrow\/sub_doors512A16.jpg\>/zastrow\/sub_doors512A16.jpg/g' $1
  sed -i '/^texture/s/\<mitaman\/zastrow\/sub_doors512B05.jpg\>/zastrow\/sub_doors512B05.jpg/g' $1
  sed -i '/^texture/s/\<mitaman\/zastrow\/sub_window31.jpg\>/zastrow\/sub_window31.jpg/g' $1
  sed -i '/^texture/s/\<mitaman\/zastrow\/sub_window33.jpg\>/zastrow\/sub_window33.jpg/g' $1
  sed -i '/^texture/s/\<sub\/sub_sand.jpg\>/zastrow\/sub_sand.jpg/g' $1
  sed -i '/^texture/s/\<sub\/brick_wall_08.jpg\>/zastrow\/brick_wall_08.jpg/g' $1
  sed -i '/^texture/s/\<sub\/brick_wall_09.jpg\>/zastrow\/brick_wall_09.jpg/g' $1

  echo -e "\a\nConversion finished!"
  echo "Please check your map config file carefully for any inconsistencies."
else
  if [ "$1" = "" ]; then
    echo -e "\a\E[31m\033[1mERROR:\E[0m Please append the path of your map config file to the end of this script."
    echo "For example, run:"
    echo -e "\tsh convert_mapconfig.sh ../packages/maps/yourmapname.cfg"
  else
    echo -e "\a\E[31m\033[1mERROR:\E[0m The path you've stated to the map config file is non-readable and/or non-writeable and/or incorrect."
    exit
  fi
fi
