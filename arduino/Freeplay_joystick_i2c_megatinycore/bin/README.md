avrdude cloned from https://github.com/avrdudes/avrdude and buit with

	cmake -D CMAKE_BUILD_TYPE=RelWithDebInfo -B build_linux -D HAVE_LINUXGPIO=OFF -D HAVE_LINUXSPI=OFF -D HAVE_LIBFTDI=OFF -D HAVE_LIBHID=OFF -D HAVE_LIBUSB=OFF -D HAVE_LIBELF=OFF -D HAVE_LIBREADLINE=OFF -D HAVE_PARPORT=OFF -D HAVE_LIBUSB_1_0=OFF

	cmake --build build_linux/
