#!/usr/bin/env bash

SOURCE=${BASH_SOURCE[0]}
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  SOURCE_DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
  SOURCE=$(readlink "$SOURCE")
  [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
SOURCE_DIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )

BUILD_DIR=/home/pi/Freeplay/fpjoy/
CONTENTS_DIR=$BUILD_DIR/contents

rm -rf $BUILD_DIR
mkdir -p $CONTENTS_DIR
#cp -rL $SOURCE_DIR/binary_package/contents/* $CONTENTS_DIR
cp -rL $SOURCE_DIR/binary_package/* $BUILD_DIR
cd $SOURCE_DIR/../arduino/Freeplay_joystick_i2c_megatinycore
make clean
make compile
cd $SOURCE_DIR/../uhid/gamepad
./build-gamepad.sh

cd $SOURCE_DIR/../uhid/digital
make

cd $SOURCE_DIR/../uhid/analog
make

cd $SOURCE_DIR/../kernel
make clean

mkdir -p $CONTENTS_DIR/arduino/Freeplay_joystick_i2c_megatinycore/Freeplay_joystick_i2c_megatinycore_build_1627/
mkdir -p $CONTENTS_DIR/arduino/Freeplay_joystick_i2c_megatinycore/bin/
mkdir -p $CONTENTS_DIR/uhid/gamepad/
mkdir -p $CONTENTS_DIR/uhid/digital/
mkdir -p $CONTENTS_DIR/uhid/analog/
mkdir -p $CONTENTS_DIR/kernel/
cp -L $SOURCE_DIR/../arduino/Freeplay_joystick_i2c_megatinycore/Freeplay_joystick_i2c_megatinycore_build_1627/Freeplay_joystick_i2c_megatinycore.ino.hex $CONTENTS_DIR/arduino/Freeplay_joystick_i2c_megatinycore/Freeplay_joystick_i2c_megatinycore_build_1627/
cp -L $SOURCE_DIR/../arduino/Freeplay_joystick_i2c_megatinycore/bin/avrdude* $CONTENTS_DIR/arduino/Freeplay_joystick_i2c_megatinycore/bin/
cp -L $SOURCE_DIR/../arduino/Freeplay_joystick_i2c_megatinycore/*.sh $CONTENTS_DIR/arduino/Freeplay_joystick_i2c_megatinycore/
cp -L $SOURCE_DIR/../uhid/gamepad/uhid-i2c-gamepad $CONTENTS_DIR/uhid/gamepad/
cp -L $SOURCE_DIR/../uhid/gamepad/uhid-i2c-gamepad-diag $CONTENTS_DIR/uhid/gamepad/
cp -L $SOURCE_DIR/../uhid/gamepad/post_init_message.txt $CONTENTS_DIR/uhid/gamepad/
cp -L $SOURCE_DIR/../uhid/digital/uhid-i2c-digital-joystick $CONTENTS_DIR/uhid/digital/
cp -L $SOURCE_DIR/../uhid/analog/uhid-i2c-analog-joystick $CONTENTS_DIR/uhid/analog/
cp -rL $SOURCE_DIR/../kernel/* $CONTENTS_DIR/kernel/
cp -L $SOURCE_DIR/../uhid/gamepad/README.md $CONTENTS_DIR/uhid/gamepad/

cd $BUILD_DIR/..
zip -r fpjoy_binary_package_$(date +"%Y-%m-%d-%H-%M-%S").zip fpjoy
