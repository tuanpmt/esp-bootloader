#
# Makefile for rBoot
# https://github.com/raburton/esp8266
#

ESPTOOL2 ?= esptool2/esptool2
ESPTOOL ?= /tools/esp8266/esptool/esptool.py
SDK_BASE ?= /tools/esp8266/sdk/ESP8266_NONOS_SDK
SPI_SIZE ?= 4M
ESPBOOT_BUILD_BASE ?= build
ESPBOOT_FW_BASE    ?= firmware
ESPBOOT_EXTRA_INCDIR ?= include
ESPPORT ?= /dev/tty.SLAB_USBtoUART

XTENSA_TOOLS_ROOT ?=


CC		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
AR		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-ar
LD		:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-gcc
OBJCOPY	:= $(XTENSA_TOOLS_ROOT)xtensa-lx106-elf-objcopy


CFLAGS    = -Os -O3 -Wpointer-arith -Wundef -Werror -Wl,-EL -fno-inline-functions -nostdlib -mlongcalls -mtext-section-literals  -D__ets__ -DICACHE_FLASH
LDFLAGS   = -nostdlib -Wl,--no-check-sections -u call_user_start -Wl,-static -L ld
LD_SCRIPT = eagle.app.v6.ld

E2_OPTS = -quiet -bin -boot0

ifeq ($(SPI_SIZE), 256K)
	E2_OPTS += -256
else ifeq ($(SPI_SIZE), 512K)
	E2_OPTS += -512
else ifeq ($(SPI_SIZE), 1M)
	E2_OPTS += -1024
else ifeq ($(SPI_SIZE), 2M)
	E2_OPTS += -2048
else ifeq ($(SPI_SIZE), 4M)
	E2_OPTS += -4096
endif

ESPBOOT_EXTRA_INCDIR := $(addprefix -I,$(ESPBOOT_EXTRA_INCDIR))

.SECONDARY:

all: $(ESPBOOT_BUILD_BASE) $(ESPBOOT_FW_BASE) $(ESPBOOT_FW_BASE)/espboot.bin


$(ESPBOOT_BUILD_BASE):
	mkdir -p $@

$(ESPBOOT_FW_BASE):
	mkdir -p $@

$(ESPBOOT_BUILD_BASE)/ram_loader.o: src/ram_loader.c include/espboot.h
	@echo "CC $<"
	$(CC) $(CFLAGS) $(ESPBOOT_EXTRA_INCDIR) -c $< -o $@
	
$(ESPBOOT_BUILD_BASE)/ram_loader.elf: $(ESPBOOT_BUILD_BASE)/ram_loader.o
	@echo "LD $@"
	@$(LD) -Tld/ram_loader.ld $(LDFLAGS) -Wl,--start-group $^ -Wl,--end-group -o $@

$(ESPBOOT_BUILD_BASE)/ram_loader.h: $(ESPBOOT_BUILD_BASE)/ram_loader.elf esptool2
	@echo "FW $@"
	@$(ESPTOOL2) -quiet -header $< $@ .text


$(ESPBOOT_BUILD_BASE)/espboot.o: src/espboot.c include/espboot.h include/user_config.h $(ESPBOOT_BUILD_BASE)/ram_loader.h
	@echo "CC $<"
	@$(CC) $(CFLAGS) -I$(ESPBOOT_BUILD_BASE) $(ESPBOOT_EXTRA_INCDIR) -c $< -o $@

$(ESPBOOT_BUILD_BASE)/%.o: %.c %.h
	@echo "CC $<"
	@$(CC) $(CFLAGS) $(ESPBOOT_EXTRA_INCDIR) -c $< -o $@

$(ESPBOOT_BUILD_BASE)/%.elf: $(ESPBOOT_BUILD_BASE)/%.o
	@echo "LD $@"
	@$(LD) -T$(LD_SCRIPT) $(LDFLAGS) -Wl,--start-group $^ -Wl,--end-group -o $@

$(ESPBOOT_FW_BASE)/%.bin: $(ESPBOOT_BUILD_BASE)/%.elf esptool2
	@echo "FW $@"
	@$(ESPTOOL2) $(E2_OPTS) $< $@ .text .rodata


esptool2:
	@make -C esptool2

flash: 
	$(ESPTOOL) -p $(ESPPORT) write_flash 0x00000 firmware/espboot.bin -fs 32m 
#0x100000 ../bootloader-fw/firmware/app.bin 
clean:
	@echo "RM $(ESPBOOT_BUILD_BASE) $(ESPBOOT_FW_BASE)"
	@rm -rf $(ESPBOOT_BUILD_BASE)
	@rm -rf $(ESPBOOT_FW_BASE)
