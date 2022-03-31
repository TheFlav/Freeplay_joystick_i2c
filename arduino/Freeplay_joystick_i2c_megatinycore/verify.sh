#!/bin/bash
sudo systemctl stop hciuart.service
sudo systemctl stop bluetooth.service
sudo rmmod hci_uart
sudo rmmod bnep btbcm bluetooth
raspi-gpio set 30-33 ip
raspi-gpio set 14 a0
raspi-gpio set 15 a0 pu
raspi-gpio set 11 op dh

if [ ${CHIP} != 817 ]; then
  CHIP=1627
  echo Setting CHIP as ${CHIP}
else
  echo Using CHIP as ${CHIP}
fi

INOFILEBIN=./Freeplay_joystick_i2c_megatinycore_build_${CHIP}/Freeplay_joystick_i2c_megatinycore.ino.bin
READFILEBASE=./Freeplay_joystick_i2c_megatinycore_build_${CHIP}/verify
READFILEHEX=$READFILEBASE.hex
READFILEBIN=$READFILEBASE.bin

if [[ ! -f "$INOFILEBIN" ]]; then
    echo "$INOFILEBIN does not exist!"
    exit 1
fi


rm -f $READFILEHEX
rm -f $READFILEBIN
pymcuprog -d attiny${CHIP} -t uart -u /dev/ttyAMA0 read -m flash -f $READFILEHEX
if [ $? -ne 0 ]
then
   echo "Read failed!"
   exit 1
fi

binfilesize=$(wc -c $INOFILEBIN | awk '{print $1}')
hex2bin.py $READFILEHEX $READFILEBIN
cmp -n $binfilesize $READFILEBIN $INOFILEBIN
if [ $? -ne 0 ]
then
   echo "Compare failed!"
   exit 1
fi

echo "Compare found no differences!"
exit 0
