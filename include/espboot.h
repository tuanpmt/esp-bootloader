#ifndef __ESPBOOT_H__
#define __ESPBOOT_H__

typedef int int32;
typedef unsigned int uint32;
typedef unsigned char uint8;

// uncomment to include .irom0.text section in the checksum
// roms must be built with esptool2 using -iromchksum option
//#define BOOT_IROM_CHKSUM

extern uint32 SPIRead(uint32 addr, void *outptr, uint32 len);
extern uint32 SPIEraseSector(int);
extern uint32 SPIWrite(uint32 addr, void *inptr, uint32 len);
extern void ets_printf(char*, ...);
extern void ets_delay_us(int);
extern void ets_memset(void*, uint8, uint32);
extern void ets_memcpy(void*, const void*, uint32);
extern uart_div_modify(int, int);
typedef void loader(void);
typedef void ramloader(uint32);
#ifndef INFO
#define INFO        ets_printf
#endif

//#define OK	1
//#define FAIL 0

#define CHKSUM_INIT 							0xEF

#define SECTOR_SIZE 							0x1000
#define BOOT_CONFIG_SECTOR 				1
#define BOOT_CONFIG_MAGIC 				0xF01A
#define DEFAULT_APPROM_ADDR				0x2000
#define DEFAULT_BACKUPROM_ADDR 		0x300000
#define BIN_MAGIC_FLASH   				0xE9
#define BIN_MAGIC_IROM    				0xEA
#define BIN_MAGIC_IROM_COUNT 			0x04
#define BUFFER_SIZE 							0x100
#define NOINLINE __attribute__ ((noinline))
typedef struct {
	uint32 magic;
	uint32 app_rom_addr;
	uint32 new_rom_addr;
	uint32 backup_rom_addr;
	uint8 chksum;
} espboot_cfg;

// standard rom header
typedef struct {
	// general rom header
	uint8 magic;
	uint8 count;
	uint8 flags1;
	uint8 flags2;
	loader* entry;
} rom_header;


typedef struct {
	uint8* address;
	uint32 length;
} section_header;

// new rom header (irom section first) there is
// another 8 byte header straight afterward the
// standard header
typedef struct {
	// general rom header
	uint8 magic;
	uint8 count; // second magic for new header
	uint8 flags1;
	uint8 flags2;
	uint32 entry;
	// new type rom, lib header
	uint32 add; // zero
	uint32 len; // length of irom section
} rom_header_new;

#define ERROR(msg) INFO("ERROR: "msg);while(1);


#endif
