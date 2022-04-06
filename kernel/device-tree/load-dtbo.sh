#sudo dtoverlay -v freeplay-joystick interrupt=40 addr=0x30 analogsticks=2 digitalbuttons=13 dpads=0 joy1-x-min=0x1A0 joy1-x-max=0xCC0 joy1-y-min=0x1A0 joy1-y-max=0xD40 joy0-x-fuzz=20 joy0-x-flat=100 joy0-swapped-x-y=1 joy1-swapped-x-y=1
#sudo dtoverlay -v freeplay-joystick interrupt=40 addr=0x30 analogsticks=2 digitalbuttons=13 dpads=0 joy1-x-min=0x1A0 joy1-x-max=0xCC0 joy1-y-min=0x1A0 joy1-y-max=0xD40 joy0-x-fuzz=20 joy0-x-flat=100
sudo dtoverlay -v freeplay-joystick interrupt=40 addr=0x30

