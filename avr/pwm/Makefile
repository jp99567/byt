PRG            = pwm
MCU_TARGET     = atmega8
OPTIMIZE       = -O2

FW_GIT_HASH_SHORT := $(shell git rev-parse --short=8 HEAD)
FW_BUID_EPOCH := $(shell date '+%s')
$(shell git diff --quiet)
FW_GIT_IS_DIRTY := $(.SHELLSTATUS)

DEFS           = -DFW_GIT_HASH_SHORT=0x$(FW_GIT_HASH_SHORT) -DFW_GIT_IS_DIRTY=$(FW_GIT_IS_DIRTY) -DFW_BUID_EPOCH=$(FW_BUID_EPOCH)
LIBS           =
BOOTSTART = 0x0
# You should not have to change anything below here.

CC             = avr-gcc
CXX            = avr-g++
CXXSRCS = $(wildcard *.cpp)
CXXOBJS := $(patsubst %.cpp,%.o,$(CXXSRCS))


# Override is only needed by avr-lib build system.

CFLAGS        = -g -Wall -std=gnu++17 $(OPTIMIZE) -mmcu=$(MCU_TARGET) $(DEFS)
LDFLAGS       = -flto -Wl,-Map,$(PRG).map
CFLAGS += -MD -MP
LDFLAGS += -Wl,--section-start=.text=$(BOOTSTART)
CXXFLAGS = $(CFLAGS)

OBJCOPY        = avr-objcopy
OBJDUMP        = avr-objdump

all: $(PRG).elf lst text eeprom

$(PRG).elf: $(CXXOBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

$(CXXOBJS): %.o : %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@
	
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
