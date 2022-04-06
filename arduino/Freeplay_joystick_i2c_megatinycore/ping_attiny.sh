sudo systemctl stop hciuart.service 
sudo systemctl stop bluetooth.service 
sudo rmmod hci_uart 
sudo rmmod bnep btbcm bluetooth
raspi-gpio set 30-33 ip
raspi-gpio set 14 a0
raspi-gpio set 15 a0 pu

raspi-gpio set 11 op dh

echo "Requires pymcuprog from https://pypi.org/project/pymcuprog/"

if [ ${CHIP} != 817 ]; then
  CHIP=1627
  echo Setting CHIP as ${CHIP}
else
  echo Using CHIP as ${CHIP}
fi


pymcuprog -d attiny${CHIP} -t uart -u /dev/ttyAMA0 ping
