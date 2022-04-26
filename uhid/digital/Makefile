#gcc -o uhid-i2c-digital-joystick uhid-i2c-digital-joystick.c -l:libi2c.a -l:libgpiod.a

CC=gcc
CFLAGS=-I.
DEPS = 
OBJ = uhid-i2c-digital-joystick.o
LIBS=-lm -l:libi2c.a -l:libgpiod.a
TARGET=uhid-i2c-digital-joystick

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f *.o *~ core $(TARGET)
