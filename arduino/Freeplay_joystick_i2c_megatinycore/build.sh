if [ "${CHIP}" != "817" ]; then
  CHIP=1627
  echo Setting CHIP as ${CHIP}
else
  echo Using CHIP as ${CHIP}
fi
bin/arduino-cli compile -v -b megaTinyCore:megaavr:atxy7:chip=${CHIP},clock=10internal,bodvoltage=1v8,bodmode=disabled,eesave=enable,millis=enabled,resetpin=UPDI,startuptime=0 --output-dir ./Freeplay_joystick_i2c_megatinycore_build_${CHIP}
