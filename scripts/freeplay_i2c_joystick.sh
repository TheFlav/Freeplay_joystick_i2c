#!/usr/bin/env bash
#this is a script that should live in /home/pi/RetroPie-Setup/scriptmodules/supplementary and can download/install the latest FPJOY binary package

rp_module_id="freeplay_i2c_joystick"
rp_module_desc="Freeplay i2c Joystick driver and firmware."
rp_module_help="Use this to download and install the latest Freeplay Joystick binary package"
rp_module_help+="More info at https://github.com/TheFlav/Freeplay_joystick_i2c"
rp_module_licence="Freeplaytech https://github.com/TheFlav/Freeplay_joystick_i2c/blob/main/LICENSE"
rp_module_section="driver"
rp_module_flags="noinstclean !x86 !osmc !xbian !mali !kms"

function depends_freeplay_i2c_joystick() {
    getDepends i2c-tools
}

function install_bin_freeplay_i2c_joystick() {
    FPBINPACK=$(curl -s https://api.github.com/repos/TheFlav/Freeplay_joystick_i2c/releases/latest | grep 'browser_' | cut -d\" -f4)
    downloadAndExtract "$FPBINPACK" "/tmp"

    diff /tmp/fpjoy/contents/builddate.txt /home/pi/Freeplay/Freeplay_joystick_i2c/builddate.txt
    if [ "$?" == "0" ]; then
      echo "Build dates match.  No need to upgrade."
      return
    fi

    sudo killall uhid-i2c-gamepad

    cp -r /home/pi/Freeplay/Freeplay_joystick_i2c /home/pi/Freeplay/Freeplay_joystick_i2c_BAK_$(date +"%Y-%m-%d-%H-%M-%S")
    /tmp/fpjoy/install_binary_package.sh

    sudo /home/pi/Freeplay/Freeplay_joystick_i2c/uhid/gamepad/uhid-i2c-gamepad --quiet > /dev/null &

    sleep 5
}

function configure_freeplay_i2c_joystick() {
    addPort "$md_id" "Freeplay i2c Joystick" "Dowload latest driver binary package for Freeplay i2c Joystick" "pushd /home/pi/Freeplay/Freeplay_joystick_i2c/scripts; /home/pi/Freeplay/Freeplay_joystick_i2c/scripts/download_install_latest_binary_package.sh; popd"
}
