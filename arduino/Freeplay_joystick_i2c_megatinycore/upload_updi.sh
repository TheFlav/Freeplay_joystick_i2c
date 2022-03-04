sudo systemctl stop hciuart.service 
sudo systemctl stop bluetooth.service 
sudo rmmod hci_uart 
sudo rmmod bnep btbcm bluetooth
raspi-gpio set 30-33 ip
raspi-gpio set 14 a0
raspi-gpio set 15 a0 pu
raspi-gpio set 11 op dh

#arduino-cli upload -b megaTinyCore:megaavr:atxy7:chip=817,clock=10internal,bodvoltage=1v8,bodmode=disabled,eesave=enable,millis=enabled,resetpin=UPDI,startuptime=0,uartvoltage=skip -p /dev/ttyAMA0 -P serialupdi57k -t
bin/arduino-cli upload -b megaTinyCore:megaavr:atxy7:chip=1627,clock=10internal,bodvoltage=1v8,bodmode=disabled,eesave=enable,millis=enabled,resetpin=UPDI,startuptime=0 -p /dev/ttyAMA0 -P serialupdi -t
