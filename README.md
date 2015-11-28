**esp-bootloader**
==========
Creating 2 firmware using 2 linker script is not convenient at all, sdk bootloader and rboot only use a maximum of 512KB flash, because one half must for 2nd firmware, esp-bootloader resolve this, it will automatically copy the new firmware and overwrite the old location when checking new firmware is valid. Of course, you will get 1Mbytes Flash and just create one single application firmware with only one linker script.
This example use 4Mbytes flash, and this is addressed layout:

```
{	
	[Bootloader] 		0x0000 - 0x0FFF: 4KB
	[Boot-config]		0x1000 - 0x1FFF: 4KB
	[app-address] 		0x2000 - 0xFFFFF: 1M-4KB
} => First 1MByte
[userSpace]			0x100000 - 0x1FFFFF => second 1 MBytes space:  user config location
[newAppSpace]		0x200000 - 0x2FFFFF => next 1 MBytes space: for new firmawre store
[BackupSpace]		0x300000 - 0x3FFFFF => Last 1 MByte space : backup firmware for ota failed
```

- App firmware linker script from Espressif, and change irom0_o_seg to`org=0x40202010`:
```
MEMORY
{
  dport0_0_seg :                        org = 0x3FF00000, len = 0x10
  dram0_0_seg :                         org = 0x3FFE8000, len = 0x14000
  iram1_0_seg :                         org = 0x40100000, len = 0x8000
  irom0_0_seg :                         org = 0x40202010, len = 0x6A000
}
```
- use esp_conn to download firmware generate by sdk for bootloader 1.4, and save to address 0x200000
- Save espboot_cfg too address 0x1000, new_rom_addr = 0x200000, and restart, bootloader will loader new rom
```
#define BOOT_CONFIG_SECTOR 				1
#define BOOT_CONFIG_MAGIC 				0xF01A
#define DEFAULT_APPROM_ADDR				0x2000
#define DEFAULT_NEWROM_ADDR				0x200000
#define DEFAULT_BACKUPROM_ADDR 		0x300000

typedef struct {
	uint32 magic;
	uint32 app_rom_addr;
	uint32 new_rom_addr;
	uint32 backup_rom_addr;
	uint8 chksum;
} espboot_cfg;
static void ICACHE_FLASH_ATTR
save_boot_cfg(espboot_cfg *cfg)
{
	cfg->chksum = calc_chksum((uint8*)cfg, (uint8*)&cfg->chksum);
	if (SPIEraseSector(BOOT_CONFIG_SECTOR) != 0)
	{
		INFO("Can not erase boot configuration sector\r\n");
	}
	if (SPIWrite(BOOT_CONFIG_SECTOR * SECTOR_SIZE, cfg, sizeof(espboot_cfg)) != 0)
	{
			INFO("Can not save boot configurations\r\n");
	}
}

static void ICACHE_FLASH_ATTR
load_boot_cfg(espboot_cfg *cfg)
{
	if (SPIRead(BOOT_CONFIG_SECTOR * SECTOR_SIZE, cfg, sizeof(espboot_cfg)) != 0)
	{
			INFO("Can not read boot configurations\r\n");
	}
	INFO("ESPBOOT: Load");
	if(cfg->magic != BOOT_CONFIG_MAGIC || cfg->chksum != calc_chksum((uint8*)cfg, (uint8*)&cfg->chksum))
	{
		INFO(" default");
		cfg->magic = BOOT_CONFIG_MAGIC;
		cfg->app_rom_addr = DEFAULT_APPROM_ADDR;
		cfg->backup_rom_addr = DEFAULT_BACKUPROM_ADDR;
		cfg->new_rom_addr = 0;
	}
	INFO(" boot settings\r\n");
}

```

 
*Based on (thanks):* [https://github.com/raburton/esp8266/tree/master/rboot](https://github.com/raburton/esp8266/tree/master/rboot)


**LICENSE - "GNU GENERAL PUBLIC LICENSE"**

Copyright (c) 2014-2015 Tuan PM, https://twitter.com/TuanPMT
