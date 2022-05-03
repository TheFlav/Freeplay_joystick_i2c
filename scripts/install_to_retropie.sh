#!/usr/bin/env bash

SOURCE=${BASH_SOURCE[0]}
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  SCRIPTDIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
  SOURCE=$(readlink "$SOURCE")
  [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
SCRIPTDIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
CONTENTSDIR=$SCRIPTDIR/../

sudo -u pi cp -rv $CONTENTSDIR/filesystem/ /
chmod -R a+x /opt/retropie/configs/all/emulationstation/scripts/*.sh

if ! grep -Fxq "/home/pi/Freeplay/Freeplay_joystick_i2c/scripts/bootup.sh" /opt/retropie/configs/all/autostart.sh; then
	sed -i '1 i /home/pi/Freeplay/Freeplay_joystick_i2c/scripts/bootup.sh' /opt/retropie/configs/all/autostart.sh
fi

