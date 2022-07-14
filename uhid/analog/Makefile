CC=gcc
CFLAGS=-I.
DEPS = 
OBJ = uhid-i2c-analog-joystick.o
LIBS=-lm -l:libi2c.a
TARGET=uhid-i2c-analog-joystick

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f *.o *~ core $(TARGET)
