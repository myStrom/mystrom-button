#!/bin/bash

if [ $# != 2 ]
then
	echo "WB ROM builder"
	echo "Usage path-to-upgrade-file.bin 4MB-flash-rom-file.bin"
	exit 1
fi

IN_FILE="$1"
OUT_FILE="$2"

rm -f $OUT_FILE

#flash size 2MB
for ((i=0;i<32;i++))
do
	dd if='0xff.bin' of=$OUT_FILE bs=$((0x1000)) conv=notrunc oflag=append
done

#bootloader
dd if='myboot.bin' of="$OUT_FILE" seek=0 skip=0 bs=$((0x1000)) conv=notrunc
#0x1000 - Bank A
dd if="$IN_FILE" of="$OUT_FILE" seek=1 skip=0 bs=$((0x1000)) conv=notrunc
#0x101000 - Bank B
dd if="$IN_FILE" of="$OUT_FILE" seek=$((0x101)) skip=0 bs=$((0x1000)) conv=notrunc
#0x1FC000 - esp_init data
dd if='esp_init_data_default_button.bin' of=$OUT_FILE seek=$((0x1FC)) skip=0 bs=$((0x1000)) conv=notrunc
#0x1FE000 - blank
dd if='blank.bin' of="$OUT_FILE" seek=$((0x1FE)) skip=0 bs=$((0x1000)) conv=notrunc
echo "Done. Exit file $OUT_FILE"
