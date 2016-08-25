#!/bin/bash
# A simple script using SED to make map configs
# compatible with the new layout.

# KSH causes conflicts, allow BASH only:
if [ -z "$(echo "$BASH")" ]; then
  echo "FAILURE: Please run this script with BASH, not SH/KSH/ZSH, etc"
  exit
fi

# HELP file:
if [ "$1" = "-h" ] || [ "$1" = "--help" ] || [ "$1" = "" ]; then
  echo "Usage: "$0" MAP_CONFIG_FILE(S)"
  echo "This script will make a backup of specified map CFG files (into their existing"
  echo "directory), then using SED it will modify the original map CFG file to be"
  echo "compatible with the new packaging layout for AssaultCube."
  echo ""
  echo "It is suggested not to use this script twice on a file, as it may duplicate the"
  echo "changes which could cause many errors."
  echo ""
  echo "This script can accept multiple map CFG files at once, as well as wild cards, as"
  echo "well as files in different directories. For example:"
  echo "    sh "$0" ../packages/maps/yourmap.cfg ../../mapfolder/*.cfg"
  echo ""
  echo "Options (must be placed before the CFG filenames):"
  echo "-h, --help    This help message."
  echo "-r, --revert  Revert specified config file(s) to the current \".BAK\" file."
  echo "-s, --strip   Strips the specified config file(s) of comments, invalid commands,"
  echo "              leading/trailing spaces/tabs and blank lines."
  echo "  -os         Same as --strip, but no compatibility conversion takes place."
  echo "  -sp         Same as --strip, but preserves any comments made before the"
  echo "              mapmodelreset command."
  echo "  -osp        Same as --strip, but preserves any comments made before the"
  echo "              mapmodelreset command AND no compatibility conversion takes place."
  exit
# REVERT command:
elif [ "$1" = "-r" ] || [ "$1" = "--revert" ]; then
  for file in "$@"; do
    if [ "$file" = "-r" ] || [ "$file" = "--revert" ]; then
      continue
    elif [[ -w "$file" && "$file" = *.cfg && -r "$file".BAK ]]; then
      mv -fv "$file".BAK "$file"
    else
      echo -e "\a\E[31m\033[1mERROR:\E[0m "$file" was unwriteable (or not a \".CFG\"), or "$file".BAK file was unreadable."
      exit
    fi
  done
  exit
fi

# Alias to define if anything failed (but the script continued).
CONTFAILED="0"

for file in "$@"; do
  if [[ -r "$file" && -w "$file" && "$file" = *.cfg ]] || [ "$file" = "-s" ] || [ "$file" = "--strip" ] || [ "$file" = "-os" ] || [ "$file" = "-sp" ] || [ "$file" = "-osp" ]; then
    if [ "$file" = "-s" ] || [ "$file" = "--strip" ] || [ "$file" = "-os" ] || [ "$file" = "-sp" ] || [ "$file" = "-osp" ]; then
      continue
    else
      # Backup files first:
      cp "$file" "$file".BAK
      if [ -r "$file".BAK ]; then
        echo "A successful backup of "$file" has been made to "$file".BAK"
        # Convert to UNIX new-line format, just in case.
        sed -i 's/^M$//' $file
        # This if-statement "strips" the files...
        if [ "$1" = "-s" ] || [ "$1" = "--strip" ] || [ "$1" = "-os" ] || [ "$1" = "-sp" ] || [ "$1" = "-osp" ]; then
          echo "Now stripping "$file" of comments, invalid commands, leading/trailing spaces/tabs and blank lines..."
          # 1) Remove comments.
          # 2) Remove unrequired "./" referencing of files.
          # 3) Remove leading/trailing spaces/tabs.
          # 4) Show ONLY lines starting with: loadnotexture loadsky mapmodelreset mapmodel texturereset texture fog fogcolour mapsoundreset mapsound watercolour shadowyaw
          # 5) Remove blank lines.
          # 6) IF ENABLED: Preserve comments made before the "mapmodelreset" command.
          sed -i 's/\/\/..*//g' $file
          sed -i 's/ ".\// "/g' $file
          sed -i 's/^[ \t]*//;s/[ \t]*$//' $file
          sed -ni '/^loadnotexture\|^loadsky\|^mapmodelreset\|^mapmodel\|^texturereset\|^texture\|^fog\|^fogcolour\|^mapsoundreset\|^mapsound\|^watercolour\|^shadowyaw/p' $file
          sed -i '/^$/d' $file
          if [ "$1" = "-sp" ] || [ "$1" = "-osp" ]; then
            # Create a working file...
            cp $file $file.tmp
            # Grab data before the "mapmodelreset" command (excluding that line) from the original file...
            sed '/^mapmodelreset/q' $file.BAK | sed '$d' > $file
            # Remove leading/trailing spaces/tabs from it...
            sed -i 's/^[ \t]*//;s/[ \t]*$//' $file
            # Keep only comments/new-lines from it...
            sed -ni '/^\/\/\|^$/p' $file
            # Paste/remove working file into normal file...
            cat $file.tmp >> $file && rm -f $file.tmp
          fi
        fi
        if [ "$1" = "-os" ] || [ "$1" = "-osp" ]; then
          echo -e "As requested, "$file" has been stripped and no compatibility conversion was made.\n"
          continue
        fi
      else
        echo -e "\a\E[31m\033[1mERROR:\E[0m Backup of "$file" failed. Please check files can be written in the \"`dirname $file`\" directory."
        echo -e "Please note, because backing up of "$file" failed, it was NOT converted and/or stripped.\n"
        CONTFAILED="1"
        continue
      fi
    fi

    # Check if this file has been converted before. If so, ask the user to be careful.
    CHECKIFDONE=`sed -n '/\/\/ This config file was converted by/p' $file | sed 's/was converted by..*//g'`
    if [ "$CHECKIFDONE" = "// This config file " ]; then
      echo -e "\n\aChecks indicate that this file has been converted before."
      echo "Continuing any further may cause this script to re-convert already converted parts of the map config."
      echo "This would cause many errors. Are you sure you wish to continue (Y/N)?"
      read ANSR
      if [ "$ANSR" = "n" ] || [ "$ANSR" = "N" ] || [ "$ANSR" = "" ]; then
        CONTFAILED="1"
        echo -e "\a\E[31m\033[1mERROR:\E[0m You've selected not to convert "$file", as it has already been converted."
        continue
      fi
    fi

    # Remove the old comment before adding the new one...
    sed -i '/This config file was converted/d' $file
    echo -e "\n// This config file was converted by "$0" on `date +%c`." >> $file

    echo "Now converting "$file" to a compatible map config file..."
    echo "Converting mapmodels..."
    sed -i '/mapmodel/s/\<laptop1\>/jcdpc\/laptop/1' $file
    sed -i '/mapmodel/s/\<rattrap\/cbbox\>/toca\/cardboardbox/1' $file
    sed -i '/mapmodel/s/\<rattrap\/rbbox\>/ratboy\/toca_cardboardbox_reskin/1' $file
    sed -i '/mapmodel/s/\<rattrap\/hanginlamp\>/jcdpc\/hanginglamp/1' $file
    sed -i '/mapmodel/s/\<rattrap\/ventflap\>/jcdpc\/ventflap/1' $file
    sed -i '/mapmodel/s/\<rattrap\/milkcrate2\>/ratboy\/toca_milkcrate_blue/1' $file
    sed -i '/mapmodel/s/\<rattrap\/milkcrate1\>/ratboy\/toca_milkcrate_red/1' $file
    sed -i '/mapmodel/s/\<aard\>/makke\/aardapple_enginebox/1' $file
    sed -i '/toxic/!s/\<barrel\>/makke\/barrel/1' $file
    sed -i '/mapmodel/s/\<barrel2\>/makke\/barrel_fallen/1' $file
    sed -i '/mapmodel/s/\<barrel-toxic\>/makke\/barrel_toxic/1' $file
    sed -i '/mapmodel/s/\<rattrapbarrel\>/makke\/barrel_newsteel/1' $file
    sed -i '/mapmodel/s/\<rattrapbarrel2\>/makke\/barrel_newsteel_fallen/1' $file
    sed -i '/mapmodel/s/\<bench\>/makke\/bench_seat/1' $file
    sed -i '/mapmodel/s/\<bridge\>/makke\/platform/1' $file
    sed -i '/mapmodel/s/\<bridge_shine\>/makke\/platform_shine/1' $file
    sed -i '/mapmodel/s/\<bulb\>/makke\/lightbulb/1' $file
    sed -i '/mapmodel/s/\<can\>/makke\/coke_can/1' $file
    sed -i '/mapmodel/s/\<can2\>/makke\/coke_can_fallen/1' $file
    sed -i '/mapmodel/s/\<chair1\>/makke\/office_chair/1' $file
    sed -i '/mapmodel/s/\<coffeemug\>/makke\/coffee_mug/1' $file
    sed -i '/mapmodel/s/\<comp_bridge\>/makke\/platform_bridge/1' $file
    sed -i '/mapmodel/s/\<drainpipe\>/makke\/drainpipe/1' $file
    sed -i '/mapmodel/s/\<dumpster\>/makke\/dumpster/1' $file
    sed -i '/mapmodel/s/\<elektro\>/makke\/electric_meter/1' $file
    sed -i '/mapmodel/s/\<europalette\>/makke\/pallet/1' $file
    sed -i '/mapmodel/s/\<fag\>/makke\/cigarette/1' $file
    sed -i '/mapmodel/s/\<fence\>/makke\/fence_chainlink/1' $file
    sed -i '/mapmodel/s/\<fencegate_closed\>/makke\/fence_chainlink_closed_gate/1' $file
    sed -i '/mapmodel/s/\<fencegate_open\>/makke\/fence_chainlink_no_gate/1' $file
    sed -i '/mapmodel/s/\<fencepost\>/makke\/fence_chainlink_post/1' $file
    sed -i '/mapmodel/s/\<flyer\>/makke\/flyer_propaganda/1' $file
    sed -i '/mapmodel/s/\<tree01\>/makke\/flyer_environmental/1' $file
    sed -i '/mapmodel/s/\<gastank\>/makke\/fuel_tank/1' $file
    sed -i '/mapmodel/s/\<icicle\>/makke\/icicle/1' $file
    sed -i '/mapmodel/s/\<hook\>/makke\/hook/1' $file
    sed -i '/mapmodel/s/\<locker\>/makke\/locker/1' $file
    sed -i '/mapmodel/s/\<light01\>/makke\/fluorescent_lamp/1' $file
    sed -i '/mapmodel/s/\<wood01\>/makke\/broken_wood/1' $file
    sed -i '/mapmodel/s/\<wrench\>/makke\/wrench/1' $file
    sed -i '/mapmodel/s/\<strahler\>/makke\/wall_spotlight/1' $file
    sed -i '/floppy/!s/\<streetlamp\>/makke\/street_light/1' $file
    sed -i '/mapmodel/s/\<ladder_rung\>/makke\/ladder_1x/1' $file
    sed -i '/mapmodel/s/\<ladder_7x\>/makke\/ladder_7x/1' $file
    sed -i '/mapmodel/s/\<ladder_8x\>/makke\/ladder_8x/1' $file
    sed -i '/mapmodel/s/\<ladder_10x\>/makke\/ladder_10x/1' $file
    sed -i '/mapmodel/s/\<ladder_11x\>/makke\/ladder_11x/1' $file
    sed -i '/mapmodel/s/\<ladder_15x\>/makke\/ladder_15x/1' $file
    sed -i '/mapmodel/s/\<ladderx15_center3\>/makke\/ladder_15x_offset/1' $file
    sed -i '/mapmodel/s/\<gutter_h\>/cleaner\/grates\/grate_hor/1' $file
    sed -i '/mapmodel/s/\<gutter_v\>/cleaner\/grates\/grate_vert/1' $file
    sed -i '/mapmodel/s/\<minelift\>/makke\/mine-shaft_elevator/1' $file
    sed -i '/mapmodel/s/\<screw\>/makke\/bolt_nut/1' $file
    sed -i '/mapmodel/s/\<sail\>/makke\/sail/1' $file
    sed -i '/mapmodel/s/\<snowsail\>/makke\/sail_snow/1' $file
    sed -i '/mapmodel/s/\<wires\/2x8\>/makke\/wires\/2x8/1' $file
    sed -i '/mapmodel/s/\<wires\/3x8\>/makke\/wires\/3x8/1' $file
    sed -i '/mapmodel/s/\<wires\/4x8\>/makke\/wires\/4x8/1' $file
    sed -i '/mapmodel/s/\<wires\/4x8a\>/makke\/wires\/4x8a/1' $file
    sed -i '/mapmodel/s/\<poster\>/makke\/signs\/wanted/1' $file
    sed -i '/mapmodel/s/\<signs\/arab\>/makke\/signs\/arab/1' $file
    sed -i '/toca/!s/\<signs\/biohazard\>/makke\/signs\/biohazard/1' $file
    sed -i '/mapmodel/s/\<signs\/caution\>/makke\/signs\/caution_voltage/1' $file
    sed -i '/mapmodel/s/\<signs\/maint\>/makke\/signs\/caution_maintainence/1' $file
    sed -i '/mapmodel/s/\<signs\/flammable\>/makke\/signs\/flammable/1' $file
    sed -i '/mapmodel/s/\<signs\/speed\>/makke\/signs\/speed/1' $file
    sed -i '/mapmodel/s/\<nocamp\>/makke\/signs\/no_camping/1' $file
    sed -i '/mapmodel/s/\<roadblock01\>/makke\/roadblock/1' $file
    sed -i '/mapmodel/s/\<roadblock02\>/makke\/roadblock_graffiti/1' $file
    sed -i '/mapmodel/s/\<nothing\>/makke\/nothing_clip/1' $file
    sed -i '/mapmodel/s/\<picture1\>/makke\/picture/1' $file
    sed -i '/mapmodel/s/\<plant01\>/makke\/plant_leafy/1' $file
    sed -i '/mapmodel/s/\<plant01_d\>/makke\/plant_leafy_dry/1' $file
    sed -i '/mapmodel/s/\<plant01_s\>/makke\/plant_leafy_snow/1' $file
    sed -i '/mapmodel/s/\<grass01\>/makke\/grass_short/1' $file
    sed -i '/mapmodel/s/\<grass01_d\>/makke\/grass_short_dry/1' $file
    sed -i '/mapmodel/s/\<grass01_s\>/makke\/grass_short_snow/1' $file
    sed -i '/mapmodel/s/\<grass02\>/makke\/grass_long/1' $file
    sed -i '/mapmodel/s/\<grass02_d\>/makke\/grass_long_dry/1' $file
    sed -i '/mapmodel/s/\<grass02_s\>/makke\/grass_long_snow/1' $file

    echo "Converting textures..."
    sed -i '/texture/s/\<wotwot\/skin\/drainpipe.jpg\>/..\/models\/mapmodels\/wotwot\/makke_drainpipe_gritty\/skin.jpg/1' $file
    sed -i '/texture/s/\<wotwot\/skin\/commrack.jpg\>/..\/models\/mapmodels\/wotwot\/toca_commrack_dull\/skin.jpg/1' $file
    sed -i '/texture/s/\<wotwot\/skin\/monitor.jpg\>/..\/models\/mapmodels\/wotwot\/toca_monitor_dull\/skin.jpg/1' $file
    sed -i '/texture/s/\<wotwot\/skin\/milkcarton.jpg\>/..\/models\/mapmodels\/wotwot\/toca_milkcarton_dull\/skin.jpg/1' $file
    sed -i '/texture/s/\<wotwot\/skin\/guardrail2.jpg\>/..\/models\/mapmodels\/wotwot\/toca_guardrail2_dull\/skin.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/zastrow\/metal_overlaps.jpg\>/zastrow\/metal_overlaps.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/zastrow\/metal_plate_fill.jpg\>/zastrow\/metal_plate_fill.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/zastrow\/metal_siding_kinksb.jpg\>/zastrow\/metal_siding_kinksb.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/zastrow\/metal_siding_kinks.jpg\>/zastrow\/metal_siding_kinks.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/zastrow\/sub_doors512A10.jpg\>/zastrow\/sub_doors512A10.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/zastrow\/sub_doors512A16.jpg\>/zastrow\/sub_doors512A16.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/zastrow\/sub_doors512B05.jpg\>/zastrow\/sub_doors512B05.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/zastrow\/sub_window31.jpg\>/zastrow\/sub_window31.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/zastrow\/sub_window33.jpg\>/zastrow\/sub_window33.jpg/1' $file
    sed -i '/texture/s/\<sub\/sub_sand.jpg\>/zastrow\/sub_sand.jpg/1' $file
    sed -i '/texture/s/\<sub\/brick_wall_08.jpg\>/zastrow\/brick_wall_08.jpg/1' $file
    sed -i '/texture/s/\<sub\/brick_wall_09.jpg\>/zastrow\/brick_wall_09.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/various\/sub_window23.jpg\>/zastrow\/sub_window23.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/various\/vent_cap.jpg\>/zastrow\/vent_cap.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/various\/sub_window38.jpg\>/zastrow\/sub_window38.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/various\/sub_doors256nf_01.jpg\>/zastrow\/sub_doors256nf_01.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_box_07.jpg\>/zastrow\/rb_box_07.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/golgotha\/elecpanelstwo.jpg\>/golgotha\/elecpanelstwo.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/golgotha\/metal_bumps2.jpg\>/golgotha\/metal_bumps2.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/golgotha\/tunnel_ceiling.jpg\>/golgotha\/tunnel_ceiling.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/golgotha\/hhroofgray.jpg\>/golgotha\/hhroofgray.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/golgotha\/metal_bumps3.jpg\>/golgotha\/metal_bumps3.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/golgotha\/tunnel_ceiling_b.jpg\>/golgotha\/tunnel_ceiling_b.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/various\/5sqtunnelroad.jpg\>/golgotha\/5sqtunnelroad.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/3dcafe\/door07_a.jpg\>/3dcafe\/door07_a.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/3dcafe\/door07.jpg\>/3dcafe\/door07.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/3dcafe\/door10_a.jpg\>/3dcafe\/door10_a.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/3dcafe\/door10.jpg\>/3dcafe\/door10.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/3dcafe\/door12.jpg\>/3dcafe\/door12.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/3dcafe\/door15.jpg\>/3dcafe\/door15.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/3dcafe\/objects08.jpg\>/3dcafe\/objects08.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/3dcafe\/objects09_a.jpg\>/3dcafe\/objects09_a.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/3dcafe\/stone18.jpg\>/3dcafe\/stone18.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/grsites\/brick051.jpg\>/grsites\/brick051.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/grsites\/brick065.jpg\>/grsites\/brick065.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/grsites\/wood060.jpg\>/grsites\/wood060.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/various\/metal020.jpg\>/grsites\/metal020.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/various\/metal026.jpg\>/grsites\/metal026.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/various\/036metal.jpg\>/lemog\/036metal.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/various\/006metal.jpg\>/lemog\/006metal.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/various\/063bois.jpg\>/lemog\/063bois.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/various\/063bois_b.jpg\>/lemog\/063bois_b.jpg/1' $file
    sed -i '/texture/s/\<mitaman\/various\/027metal.jpg\>/lemog\/027metal.jpg/1' $file
    sed -i '/texture/s/\<makke\/windows.jpg\>/golgotha\/windows.jpg/1' $file
    sed -i '/texture/s/\<makke\/window.jpg\>/golgotha\/window.jpg/1' $file
    sed -i '/texture/s/\<makke\/panel.jpg\>/golgotha\/panel.jpg/1' $file
    sed -i '/texture/s/\<makke\/door.jpg\>/golgotha\/door.jpg/1' $file
    sed -i '/texture/s/\<makke\/smallsteelbox.jpg\>/golgotha\/smallsteelbox.jpg/1' $file
    sed -i '/texture/s/\<makke\/klappe3.jpg\>/golgotha\/klappe3.jpg/1' $file
    sed -i '/texture/s/\<makke\/bricks_2.jpg\>/mayang\/bricks_2.jpg/1' $file
    sed -i '/texture/s/\<wotwot\/urban\/manhole1.jpg\>/mayang\/manhole1.jpg/1' $file
    sed -i '/texture/s/\<wotwot\/urban\/hatch1.jpg\>/mayang\/hatch1.jpg/1' $file
    sed -i '/texture/s/\<wotwot\/urban\/grill2_s.jpg\>/mayang\/grill2_s.jpg/1' $file
    sed -i '/texture/s/\<wotwot\/urban\/door3.jpg\>/mayang\/door3.jpg/1' $file
    sed -i '/texture/s/\<wotwot\/urban\/airvent1.jpg\>/mayang\/airvent1.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_trim_03.jpg\>/golgotha\/rb_trim_03.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_window.jpg\>/makke\/rb_window.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_window2.jpg\>/makke\/rb_window2.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_trim_01.jpg\>/noctua\/ground\/rb_trim_01.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_trim_02.jpg\>/makke\/rb_trim_02.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_box_01.jpg\>/makke\/rattrap\/rb_box_01.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_box_02.jpg\>/makke\/rattrap\/rb_box_02.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_box_03.jpg\>/makke\/rattrap\/rb_box_03.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_box_04.jpg\>/makke\/rattrap\/rb_box_04.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_box_05.jpg\>/makke\/rattrap\/rb_box_05.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_box_06.jpg\>/makke\/rattrap\/rb_box_06.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_concrete.jpg\>/makke\/rattrap\/rb_concrete.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_bricks_01.jpg\>/mayang\/rb_bricks_01.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_bricks_02.jpg\>/mayang\/rb_bricks_02.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_bricks_03.jpg\>/mayang\/rb_bricks_03.jpg/1' $file
    sed -i '/texture/s/\<rattrap\/rb_planks02_trim.jpg\>/noctua\/wood\/planks02_trim_vert.jpg/1' $file

    echo -e "Successfully finished converting: "$file"\n"
  else
    echo -e "\a\E[31m\033[1mERROR:\E[0m "$file" is an incorrect filename, path or option. Or it is not a \".cfg\" file, or it may be non-readable and/or non-writeable.\n"
    CONTFAILED="1"
  fi
done

if [ "$CONTFAILED" = "1" ]; then
  echo -e "\a\nConversion finished, HOWEVER, \E[31m\033[1msome files were NOT converted and/or stripped.\E[0m!"
  echo "Please check the output of this script for their errors!"
  echo "It is suggested to check your map config files carefully for any inconsistencies, using the \"diff\" command."
else
  echo -e "\aConversion succeessfully completed with NO errors!"
  echo "It is suggested	to check your map config files carefully for any inconsistencies, using	the \"diff\" command."
fi
