all:
	@avr-gcc \
		*.c \
		-mmcu=atmega32u4 \
		-Os \
		-DF_CPU=8000000UL \
		-std=gnu99 \
		-I./cab202_teensy \
		-L./cab202_teensy \
		-I./usb_serial \
		-I./cab202_adc \
		./usb_serial/usb_serial.o \
		./cab202_adc/cab202_adc.o \
		-lcab202_teensy \
		-Wl,-u,vfprintf \
		-lprintf_flt \
		-lm \
		-o a2_n9625607.obj
	@avr-objcopy -O ihex a2_n9625607.obj a2_n9625607.hex

upload:
	@echo "PRESS BUTTON ON TEENSY!"
	@teensy_loader_cli -mmcu=atmega32u4 -w a2_n9625607.hex
	@echo "DONE :D"

clean: 
	  $(RM) a2_n9625607.*