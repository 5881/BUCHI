project.hex: project
	avr-objcopy -O ihex project project.hex
	
project:
	avr-gcc -std=gnu99 -mmcu=atmega8 -O2 -o project main.c

write: project.hex
	avrdude -c arduino-ft232r -pm8 -b 115200 -U flash:w:project.hex
	#sudo avrdude -c nikolaew -pm8  -P /dev/ttyS0 -U flash:w:project.hex
clean:
	rm project project.hex
all: clean project.hex write

fuse:
	avrdude -c arduino-ft232r -pm8 -v -v  -b 1200  -U lfuse:w:0xFF:m  -U hfuse:w:0xC9:m
	
