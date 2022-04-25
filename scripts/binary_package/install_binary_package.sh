#!/usr/bin/env bash

SOURCE=${BASH_SOURCE[0]}
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  SCRIPTDIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
  SOURCE=$(readlink "$SOURCE")
  [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
SCRIPTDIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
CONTENTSDIR=$SCRIPTDIR/contents

DESTDIR=/home/pi/Freeplay/Freeplay_joystick_i2c/

mkdir -p $DESTDIR/arduino/Freeplay_joystick_i2c_megatinycore/Freeplay_joystick_i2c_megatinycore_build_1627/
mkdir -p $DESTDIR/arduino/Freeplay_joystick_i2c_megatinycore/bin/
cp $CONTENTSDIR/Freeplay_joystick_i2c_megatinycore.ino.hex $DESTDIR/arduino/Freeplay_joystick_i2c_megatinycore/Freeplay_joystick_i2c_megatinycore_build_1627/
cp -r $CONTENTSDIR/avrdude* $DESTDIR/arduino/Freeplay_joystick_i2c_megatinycore/bin
mkdir -p $DESTDIR/uhid/gamepad
cp $CONTENTSDIR/uhid-i2c-gamepad $DESTDIR/uhid/gamepad
cp $CONTENTSDIR/uhid-i2c-gamepad-diag $DESTDIR/uhid/gamepad
mkdir -p $DESTDIR/scripts
cp $CONTENTSDIR/bootup.sh $DESTDIR/scripts
$CONTENTSDIR/install_to_retropie.sh
$CONTENTSDIR/setup_linux.sh

chown pi:pi /home/pi/Freeplay/
chown -R pi:pi $DESTDIR
