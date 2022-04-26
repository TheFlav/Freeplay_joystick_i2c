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

mkdir -p $DESTDIR
cp -r $CONTENTSDIR/* $DESTDIR

chown pi:pi /home/pi/Freeplay/
chown -R pi:pi $DESTDIR
