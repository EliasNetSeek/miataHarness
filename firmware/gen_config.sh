#!/bin/bash

echo "This script reads rusefi_config.txt and produces firmware persistent configuration headers"
echo "The storage section of rusefi.ini is updated as well"

rm -f gen_config.log
rm -f gen_config_board.log

# todo: who is the consumer of this folder? shall we move that 'mkdir' command closer to usage?
mkdir build


# todo: we have a bit of code duplication with build-firmware.yaml here :(
config/boards/kinetis/config/gen_kinetis_config.sh
[ $? -eq 0 ] || { echo "ERROR generating board kinetis kin"; exit 1; }

config/boards/cypress/config/gen_cypress_config.sh
[ $? -eq 0 ] || { echo "ERROR generating board cypress cypress"; exit 1; }

config/boards/subaru_eg33/config/gen_subaru_config.sh
[ $? -eq 0 ] || { echo "ERROR generating board subaru_eg33 subaru_eg33_f7"; exit 1; }

#
# see also build-firmware where we compile all versions of firmware
# *** IMPORTANT *** most inclusive f407-discovery is last for a reason - we want it's generated files to be in repo
#
for BOARD in \
   "config/boards/hellen/alphax-2chan alphax-2chan" \
   "config/boards/hellen/alphax-4chan alphax-4chan" \
   "config/boards/hellen/alphax-4chan alphax-4chan_f7" \
   "config/boards/hellen/alphax-8chan alphax-8chan" \
   "config/boards/hellen/alphax-8chan alphax-8chan_f7" \
   "config/boards/hellen/harley81 harley81" \
   "config/boards/hellen/hellen128 hellen128 rusefi_hellen128mercedes.ini" \
   "config/boards/hellen/hellen121vag hellen121vag" \
   "config/boards/hellen/hellen121nissan hellen121nissan" \
   "config/boards/hellen/hellen-honda-k hellen-honda-k" \
   "config/boards/hellen/hellen154hyundai hellen154hyundai" \
   "config/boards/hellen/hellen88bmw hellen88bmw" \
   "config/boards/hellen/hellen72 hellen72" \
   "config/boards/hellen/hellen81 hellen81" \
   "config/boards/hellen/hellen-nb1 hellen-nb1" \
   "config/boards/hellen/hellen-gm-e67 hellen-gm-e67" \
   "config/boards/hellen/hellen64_miataNA6_94 hellenNA6" \
   "config/boards/hellen/hellenNA8_96 hellenNA8_96" \
   "config/boards/hellen/small-can-board small-can" \
   "config/boards/microrusefi mre_f7" \
   "config/boards/microrusefi mre_f4" \
   "config/boards/core8 core8" \
   "config/boards/48way 48way" \
   "config/boards/m74_9 m74_9" \
   "config/boards/s105 s105" \
   "config/boards/test-build-guards t-b-g" \
   "config/boards/frankenso frankenso_na6" \
   "config/boards/prometheus prometheus_469" \
   "config/boards/prometheus prometheus_405" \
   "config/boards/proteus proteus_f7" \
   "config/boards/proteus proteus_f4" \
   "config/boards/proteus proteus_h7" \
   "config/boards/f429-discovery f429-discovery" \
   "config/boards/nucleo_f429 nucleo_f429" \
   "config/boards/nucleo_f767 nucleo_f767" \
   "config/boards/nucleo_h743 nucleo_h743" \
   "config/boards/atlas atlas"\
   "config/boards/tdg-pdm8 tdg-pdm8"\
   "config/boards/f407-discovery f407-discovery" \
   ; do
 BOARD_NAME=$(echo "$BOARD" | cut -d " " -f 1)
 BOARD_SHORT_NAME=$(echo "$BOARD" | cut -d " " -f 2)
 INI=$(echo "$BOARD" | cut -d " " -f 3)
 ./gen_config_board.sh $BOARD_NAME $BOARD_SHORT_NAME $INI
 [ $? -eq 0 ] || { echo "ERROR generating board $BOARD_NAME $BOARD_SHORT_NAME $INI"; exit 1; }
done

exit 0
