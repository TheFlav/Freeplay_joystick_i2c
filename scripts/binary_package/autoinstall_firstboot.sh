#!/usr/bin/env bash

SOURCE=${BASH_SOURCE[0]}
while [ -h "$SOURCE" ]; do # resolve $SOURCE until the file is no longer a symlink
  SCRIPTDIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )
  SOURCE=$(readlink "$SOURCE")
  [[ $SOURCE != /* ]] && SOURCE=$DIR/$SOURCE # if $SOURCE was a relative symlink, we need to resolve it relative to the path where the symlink file was located
done
SCRIPTDIR=$( cd -P "$( dirname "$SOURCE" )" >/dev/null 2>&1 && pwd )


whiptail --infobox "Installing Freeplay i2c Joystick binary package." 20 60
$SCRIPTDIR/install_binary_package.sh
whiptail --infobox "Installed Freeplay i2c Joystick binary package." 20 60
sleep 5
/usr/lib/raspi-config/init_resize.sh
