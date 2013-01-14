#!/bin/bash
# A simple script using SED to make map configs
# compatible with the new layout.

CONTFAILED="0"

for file in $*; do
  if [[ -r "$file" && -w "$file" && "$file" = *.cfg ]]; then
    cp "$file" "$file".BAK
    if [ -r "$file".BAK ]; then
      echo "A successful backup of "$file" has been made to "$file".BAK"
      echo "Now converting "$file" to a compatible map config file..."
    else
      echo -e "\a\E[31m\033[1mERROR:\E[0m Backup of "$file" failed. Please check files can be written in the \"`dirname $file`\" directory."
      echo -e "Please note, because backing up of "$file" failed, it was NOT converted.\n"
      CONTFAILED="1"
      continue
    fi

    echo "Converting mapmodels..."
    sed -i '/^mapmodel/s/\<laptop1\>/jcdpc\/laptop/g' $file
    sed -i '/^mapmodel/s/\<rattrap\/cbbox\>/jcdpc\/cardboardbox/g' $file
    sed -i '/^mapmodel/s/\<rattrap\/hanginlamp\>/jcdpc\/hanginglamp/g' $file
    sed -i '/^mapmodel/s/\<rattrap\/ventflap\>/jcdpc\/ventflap/g' $file
    sed -i '/^mapmodel/s/\<rattrap\/milkcrate2\>/ratboy\/toca_milkcrate_blue/g' $file
    sed -i '/^mapmodel/s/\<rattrap\/milkcrate1\>/ratboy\/toca_milkcrate_red/g' $file
    sed -i '/^mapmodel/s/\<aard\>/makke\/aardapple_enginebox/g' $file
    sed -i '/toxic/!s/\<barrel\>/makke\/barrel/g' $file
    sed -i '/^mapmodel/s/\<barrel2\>/makke\/barrel_fallen/g' $file
    sed -i '/^mapmodel/s/\<barrel-toxic\>/makke\/barrel_toxic/g' $file
    sed -i '/^mapmodel/s/\<rattrapbarrel\>/makke\/barrel_newsteel/g' $file
    sed -i '/^mapmodel/s/\<rattrapbarrel2\>/makke\/barrel_newsteel_fallen/g' $file
    sed -i '/^mapmodel/s/\<bench\>/makke\/bench_seat/g' $file
    sed -i '/^mapmodel/s/\<bridge\>/makke\/platform/g' $file
    sed -i '/^mapmodel/s/\<bridge_shine\>/makke\/platform_shine/g' $file
    sed -i '/^mapmodel/s/\<bulb\>/makke\/lightbulb/g' $file
    sed -i '/^mapmodel/s/\<can\>/makke\/coke_can/g' $file
    sed -i '/^mapmodel/s/\<can2\>/makke\/coke_can_fallen/g' $file
    sed -i '/^mapmodel/s/\<chair1\>/makke\/office_chair/g' $file
    sed -i '/^mapmodel/s/\<coffeemug\>/makke\/coffee_mug/g' $file
    sed -i '/^mapmodel/s/\<comp_bridge\>/makke\/platform_bridge/g' $file
    sed -i '/^mapmodel/s/\<drainpipe\>/makke\/drainpipe/g' $file
    sed -i '/^mapmodel/s/\<dumpster\>/makke\/dumpster/g' $file
    sed -i '/^mapmodel/s/\<elektro\>/makke\/electric_meter/g' $file
    sed -i '/^mapmodel/s/\<europalette\>/makke\/pallet/g' $file
    sed -i '/^mapmodel/s/\<fag\>/makke\/cigarette/g' $file
    sed -i '/^mapmodel/s/\<fence\>/makke\/fence_chainlink/g' $file
    sed -i '/^mapmodel/s/\<fencegate_closed\>/makke\/fence_chainlink_closed_gate/g' $file
    sed -i '/^mapmodel/s/\<fencegate_open\>/makke\/fence_chainlink_no_gate/g' $file
    sed -i '/^mapmodel/s/\<fencepost\>/makke\/fence_chainlink_post/g' $file
    sed -i '/^mapmodel/s/\<flyer\>/makke\/flyer_propaganda/g' $file
    sed -i '/^mapmodel/s/\<tree01\>/makke\/flyer_environmental/g' $file
    sed -i '/^mapmodel/s/\<gastank\>/makke\/fuel_tank/g' $file
    sed -i '/^mapmodel/s/\<icicle\>/makke\/icicle/g' $file
    sed -i '/^mapmodel/s/\<hook\>/makke\/hook/g' $file
    sed -i '/^mapmodel/s/\<locker\>/makke\/locker/g' $file
    sed -i '/^mapmodel/s/\<light01\>/makke\/fluorescent_lamp/g' $file
    sed -i '/^mapmodel/s/\<wood01\>/makke\/broken_wood/g' $file
    sed -i '/^mapmodel/s/\<wrench\>/makke\/wrench/g' $file
    sed -i '/^mapmodel/s/\<strahler\>/makke\/wall_spotlight/g' $file
    sed -i '/floppy/!s/\<streetlamp\>/makke\/street_light/g' $file
    sed -i '/^mapmodel/s/\<ladder_rung\>/makke\/ladder_1x/g' $file
    sed -i '/^mapmodel/s/\<ladder_7x\>/makke\/ladder_7x/g' $file
    sed -i '/^mapmodel/s/\<ladder_8x\>/makke\/ladder_8x/g' $file
    sed -i '/^mapmodel/s/\<ladder_10x\>/makke\/ladder_10x/g' $file
    sed -i '/^mapmodel/s/\<ladder_11x\>/makke\/ladder_11x/g' $file
    sed -i '/^mapmodel/s/\<ladder_15x\>/makke\/ladder_15x/g' $file
    sed -i '/^mapmodel/s/\<ladderx15_center3\>/makke\/ladder_15x_offset/g' $file
    sed -i '/^mapmodel/s/\<gutter_h\>/makke\/grate_hor/g' $file
    sed -i '/^mapmodel/s/\<gutter_v\>/makke\/grate_vert/g' $file
    sed -i '/^mapmodel/s/\<minelift\>/makke\/mine-shaft_elevator/g' $file
    sed -i '/^mapmodel/s/\<screw\>/makke\/bolt_nut/g' $file
    sed -i '/^mapmodel/s/\<sail\>/makke\/sail/g' $file
    sed -i '/^mapmodel/s/\<snowsail\>/makke\/sail_snow/g' $file
    sed -i '/^mapmodel/s/\<wires\/2x8\>/makke\/wires\/2x8/g' $file
    sed -i '/^mapmodel/s/\<wires\/3x8\>/makke\/wires\/3x8/g' $file
    sed -i '/^mapmodel/s/\<wires\/4x8\>/makke\/wires\/4x8/g' $file
    sed -i '/^mapmodel/s/\<wires\/4x8a\>/makke\/wires\/4x8a/g' $file
    sed -i '/^mapmodel/s/\<poster\>/makke\/signs\/wanted/g' $file
    sed -i '/^mapmodel/s/\<signs\/arab\>/makke\/signs\/arab/g' $file
    sed -i '/toca/!s/\<signs\/biohazard\>/makke\/signs\/biohazard/g' $file
    sed -i '/^mapmodel/s/\<signs\/caution\>/makke\/signs\/caution_voltage/g' $file
    sed -i '/^mapmodel/s/\<signs\/maint\>/makke\/signs\/caution_maintainence/g' $file
    sed -i '/^mapmodel/s/\<signs\/flammable\>/makke\/signs\/flammable/g' $file
    sed -i '/^mapmodel/s/\<signs\/speed\>/makke\/signs\/speed/g' $file
    sed -i '/^mapmodel/s/\<nocamp\>/makke\/signs\/no_camping/g' $file
    sed -i '/^mapmodel/s/\<roadblock01\>/makke\/roadblock/g' $file
    sed -i '/^mapmodel/s/\<roadblock02\>/makke\/roadblock_graffiti/g' $file
    sed -i '/^mapmodel/s/\<nothing\>/makke\/nothing_clip/g' $file
    sed -i '/^mapmodel/s/\<picture1\>/makke\/picture/g' $file
    sed -i '/^mapmodel/s/\<plant01\>/makke\/plant_leafy/g' $file
    sed -i '/^mapmodel/s/\<plant01_d\>/makke\/plant_leafy_dry/g' $file
    sed -i '/^mapmodel/s/\<plant01_s\>/makke\/plant_leafy_snow/g' $file
    sed -i '/^mapmodel/s/\<grass01\>/makke\/grass_short/g' $file
    sed -i '/^mapmodel/s/\<grass01_d\>/makke\/grass_short_dry/g' $file
    sed -i '/^mapmodel/s/\<grass01_s\>/makke\/grass_short_snow/g' $file
    sed -i '/^mapmodel/s/\<grass02\>/makke\/grass_long/g' $file
    sed -i '/^mapmodel/s/\<grass02_d\>/makke\/grass_long_dry/g' $file
    sed -i '/^mapmodel/s/\<grass02_s\>/makke\/grass_long_snow/g' $file

    echo "Converting textures..."
    sed -i '/^texture/s/\<wotwot\/skin\/drainpipe.jpg\>/..\/models\/mapmodels\/wotwot\/makke_drainpipe_gritty\/skin.jpg/g' $file
    sed -i '/^texture/s/\<wotwot\/skin\/commrack.jpg\>/..\/models\/mapmodels\/wotwot\/toca_commrack_dull\/skin.jpg/g' $file
    sed -i '/^texture/s/\<wotwot\/skin\/monitor.jpg\>/..\/models\/mapmodels\/wotwot\/toca_monitor_dull\/skin.jpg/g' $file
    sed -i '/^texture/s/\<wotwot\/skin\/milkcarton.jpg\>/..\/models\/mapmodels\/wotwot\/toca_milkcarton_dull\/skin.jpg/g' $file
    sed -i '/^texture/s/\<wotwot\/skin\/guardrail2.jpg\>/..\/models\/mapmodels\/wotwot\/toca_guardrail2_dull\/skin.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/zastrow\/metal_overlaps.jpg\>/zastrow\/metal_overlaps.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/zastrow\/metal_plate_fill.jpg\>/zastrow\/metal_plate_fill.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/zastrow\/metal_siding_kinksb.jpg\>/zastrow\/metal_siding_kinksb.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/zastrow\/metal_siding_kinks.jpg\>/zastrow\/metal_siding_kinks.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/zastrow\/sub_doors512A10.jpg\>/zastrow\/sub_doors512A10.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/zastrow\/sub_doors512A16.jpg\>/zastrow\/sub_doors512A16.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/zastrow\/sub_doors512B05.jpg\>/zastrow\/sub_doors512B05.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/zastrow\/sub_window31.jpg\>/zastrow\/sub_window31.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/zastrow\/sub_window33.jpg\>/zastrow\/sub_window33.jpg/g' $file
    sed -i '/^texture/s/\<sub\/sub_sand.jpg\>/zastrow\/sub_sand.jpg/g' $file
    sed -i '/^texture/s/\<sub\/brick_wall_08.jpg\>/zastrow\/brick_wall_08.jpg/g' $file
    sed -i '/^texture/s/\<sub\/brick_wall_09.jpg\>/zastrow\/brick_wall_09.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/various\/sub_window23.jpg\>/zastrow\/sub_window23.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/various\/vent_cap.jpg\>/zastrow\/vent_cap.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/various\/sub_window38.jpg\>/zastrow\/sub_window38.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/various\/sub_doors256nf_01.jpg\>/zastrow\/sub_doors256nf_01.jpg/g' $file
    sed -i '/^texture/s/\<rattrap\/rb_box_07.jpg\>/zastrow\/rb_box_07.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/golgotha\/elecpanelstwo.jpg\>/golgotha\/elecpanelstwo.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/golgotha\/metal_bumps3.jpg\>/golgotha\/metal_bumps3.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/golgotha\/tunnel_ceiling.jpg\>/golgotha\/tunnel_ceiling.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/golgotha\/hhroofgray.jpg\>/golgotha\/metal_bumps2.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/golgotha\/tunnel_ceiling_b.jpg\>/golgotha\/tunnel_ceiling_b.jpg/g' $file
    sed -i '/^texture/s/\<mitaman\/various\/5sqtunnelroad.jpg\>/golgotha\/5sqtunnelroad.jpg/g' $file



    echo -e "Successfully finished converting: "$file"\n"
  else
    echo -e "\a\E[31m\033[1mERROR:\E[0m "$file" is an incorrect filename or path, or it is not a \".cfg\" file, or it may be non-readable and/or non-writeable.\n"
    CONTFAILED="1"
  fi
done

if [ "$1" = "" ]; then
  echo -e "\a\E[31m\033[1mERROR:\E[0m Please append the path of your map config file to the end of this script."
  echo "For example, run:"
  echo -e "\tsh $0 the/path/to/your/mapconfigfile.cfg"
elif [ "$CONTFAILED" = "1" ]; then
  echo -e "\aConversion finished, HOWEVER, \E[31m\033[1msome files were NOT converted\E[0m!"
  echo "Please check the output of this script for their errors!"
  echo "It is suggested to check your map config files carefully for any inconsistencies, using the \"diff\" command."
else
  echo -e "\aConversion succeessfully completed with NO errors!"
  echo "It is suggested	to check your map config files carefully for any inconsistencies, using	the \"diff\" command."
fi
