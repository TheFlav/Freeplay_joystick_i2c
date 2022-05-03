#!/bin/bash

LCDINUSE=$(/opt/vc/bin/tvservice -s | grep -c "\[LCD\]")
if [[ "$LCDINUSE" == "1" ]]; then
  i2cset -y 0 0x40 0x0a 0x55
  i2cset -y 0 0x40 0x02 0x02
  i2cset -y 0 0x40 0x0a 0xAA
fi
