#!/bin/bash
#bin/arduino-cli compile -v -b megaTinyCore:megaavr:atxy7:chip=1627,clock=10internal,bodvoltage=1v8,bodmode=disabled,eesave=enable,millis=enabled,resetpin=UPDI,startuptime=0 --output-dir ./Freeplay_joystick_i2c_megatinycore_build/
#if [ $? -ne 0 ]
#then
#   echo "Build failed!"
#   exit
#fi
sudo systemctl stop hciuart.service
sudo systemctl stop bluetooth.service
sudo rmmod hci_uart
sudo rmmod bnep btbcm bluetooth
raspi-gpio set 30-33 ip
raspi-gpio set 14 a0
raspi-gpio set 15 a0 pu
raspi-gpio set 11 op dh

INOFILEBIN=./Freeplay_joystick_i2c_megatinycore_build/Freeplay_joystick_i2c_megatinycore.ino.bin
READFILEBASE=./Freeplay_joystick_i2c_megatinycore_build/verify
READFILEHEX=$READFILEBASE.hex
READFILEBIN=$READFILEBASE.bin

rm -f $READFILEHEX
rm -f $READFILEBIN
#pymcuprog -d attiny817 -t uart -u /dev/ttyAMA0 read -m flash -f $READFILEHEX
pymcuprog -d attiny1627 -t uart -u /dev/ttyAMA0 read -m flash -f $READFILEHEX
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
