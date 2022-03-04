# Raspberry Pi UPDI Programming
  See https://github.com/SpenceKonde/AVR-Guidance/blob/master/UPDI/jtag2updi.md
  
  See https://github.com/mraardvark/pyupdi


## Wire it like this:

                                  Vcc                     Vcc
                                  +-+                     +-+
                                   |                       |
           +---------------------+ |                       | +--------------------+
           | Raspberry Pi        +-+                       +-+  AVR device        |
           |                     |      +----------+         |                    |
           |           GPIO14 TX +------+ <| Diode +---------+ UPDI               |
           |                     |      +----------+    |    |                    |
           |                     |                      |    |                    |
           |           GPIO15 RX +----------------------+    |                    |
           |                     |                           |                    |
           |                     +--+                     +--+                    |
           +---------------------+  |                     |  +--------------------+
                                   +-+                   +-+
                                   GND                   GND
                         
            
            
## Test it like this (see ping_attiny.sh):
        sudo systemctl stop hciuart.service 
        sudo systemctl stop bluetooth.service 
        sudo rmmod hci_uart 
        sudo rmmod bnep btbcm bluetooth
        raspi-gpio set 30-33 ip
        raspi-gpio set 14 a0
        raspi-gpio set 15 a0 pu
        pymcuprog -d attiny817 -t uart -u /dev/ttyAMA0 ping
                         

## Make sure the pins are like this:                        
        raspi-gpio get 14-15,30-33
         GPIO 14: level=1 fsel=4 alt=0 func=TXD0
         GPIO 15: level=1 fsel=4 alt=0 func=RXD0
         GPIO 30: level=0 fsel=0 func=INPUT
         GPIO 31: level=1 fsel=0 func=INPUT
         GPIO 32: level=1 fsel=0 func=INPUT
         GPIO 33: level=1 fsel=0 func=INPUT

## To compile the code using build.sh, first set up this:
         sudo apt install arduino-builder
         arduino-cli core update-index --additional-urls http://drazzy.com/package_drazzy.com_index.json
         arduino-cli core search megaTinyCore
         arduino-cli core install megaTinyCore:megaavr

and see "Install megaTinyCore" at https://github.com/SpenceKonde/megaTinyCore/blob/master/megaavr/extras/Arduino-cli.md
