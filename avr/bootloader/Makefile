PRG            = bootloader
OBJ            = bootloader.o
MCU_TARGET     = at90can32
OPTIMIZE       = -Os

DEFS           = -DSLAVE_SERVICE_ID=$(ID)
LIBS           =
BOOTSTART = 0x7C00
# You should not have to change anything below here.

CC             = avr-gcc
CXX            = avr-g++
SRC = $(wildcard *.c)

# Override is only needed by avr-lib build system.

CFLAGS        = -g -Wall -std=gnu++11 $(OPTIMIZE) -mmcu=$(MCU_TARGET) $(DEFS)
LDFLAGS       = -Wl,-Map,$(PRG).map
CFLAGS += -MD -MP
LDFLAGS += -Wl,--section-start=.text=$(BOOTSTART)
CXXFLAGS = $(CFLAGS)

OBJCOPY        = avr-objcopy
OBJDUMP        = avr-objdump

all: $(PRG).elf lst text eeprom

$(PRG).elf: $(OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -rf *.o *.d $(PRG).elf
	rm -rf *.lst *.map $(EXTRA_CLEAN_FILES)

lst:  $(PRG).lst

%.lst: %.elf
	$(OBJDUMP) -h -S $< > $@

# Rules for building the .text rom images

text: hex bin srec

hex:  $(PRG).hex
bin:  $(PRG).bin
srec: $(PRG).srec

%.hex: %.elf
	$(OBJCOPY) -j .text -j .data -O ihex $< $@

%.srec: %.elf
	$(OBJCOPY) -j .text -j .data -O srec $< $@

%.bin: %.elf
	$(OBJCOPY) -j .text -j .data -O binary $< $@
	ls -l *.bin

# Rules for building the .eeprom rom images

eeprom: ehex ebin esrec

ehex:  $(PRG)_eeprom.hex
ebin:  $(PRG)_eeprom.bin
esrec: $(PRG)_eeprom.srec

%_eeprom.hex: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $< $@ \
	|| { echo empty $@ not generated; exit 0; }

%_eeprom.srec: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O srec $< $@ \
	|| { echo empty $@ not generated; exit 0; }

%_eeprom.bin: %.elf
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O binary $< $@ \
	|| { echo empty $@ not generated; exit 0; }

# Every thing below here is used by avr-libc's build system and can be ignored
# by the casual user.

EXTRA_CLEAN_FILES       = *.hex *.bin *.srec
