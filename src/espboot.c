#include "espboot.h"
#include "ram_loader.h"

rom_header header;
rom_header_new header_new;
espboot_cfg boot_cfg;

// calculate checksum for block of data
// from start up to (but excluding) end
static uint8 calc_chksum(uint8 *start, uint8 *end)
{
	uint8 chksum = CHKSUM_INIT;
	while(start < end) {
		chksum ^= *start;
		start++;
	}
	return chksum;
}

static uint32 check_image(uint32 readpos, uint32 *totalSize)
{

	uint8 buffer[BUFFER_SIZE];
	uint8 sectcount;
	uint8 sectcurrent;
	uint8 *writepos;
	uint8 chksum = CHKSUM_INIT;
	uint32 loop;
	uint32 remaining;
	uint32 romaddr;


	rom_header_new *header = (rom_header_new*)buffer;
	section_header *section = (section_header*)buffer;
	*totalSize = readpos;
	if (readpos == 0 || readpos == 0xffffffff) {
		return 0;
	}

	// read rom header
	if (SPIRead(readpos, header, sizeof(rom_header_new)) != 0) {
		return 0;
	}

	// check header type
	if (header->magic == BIN_MAGIC_FLASH) {
		// old type, no extra header or irom section to skip over
		romaddr = readpos;
		readpos += sizeof(rom_header);
		sectcount = header->count;

	} else if (header->magic == BIN_MAGIC_IROM && header->count == BIN_MAGIC_IROM_COUNT) {
		// new type, has extra header and irom section first
		romaddr = readpos + header->len + sizeof(rom_header_new);

#ifdef BOOT_IROM_CHKSUM
		// we will set the real section count later, when we read the header
		sectcount = 0xff;
		// just skip the first part of the header
		// rest is processed for the chksum
		readpos += sizeof(rom_header);
#else
		// skip the extra header and irom section
		readpos = romaddr;
		// read the normal header that follows
		if (SPIRead(readpos, header, sizeof(rom_header)) != 0) {
			return 0;
		}
		sectcount = header->count;
		readpos += sizeof(rom_header);


#endif
	} else {
		return 0;
	}

	// test each section
	for (sectcurrent = 0; sectcurrent < sectcount; sectcurrent++) {

		// read section header
		if (SPIRead(readpos, section, sizeof(section_header)) != 0) {
			return 0;
		}
		readpos += sizeof(section_header);

		// get section address and length
		writepos = section->address;
		remaining = section->length;

		while (remaining > 0) {
			// work out how much to read, up to BUFFER_SIZE
			uint32 readlen = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
			// read the block
			if (SPIRead(readpos, buffer, readlen) != 0) {
				return 0;
			}
			// increment next read and write positions
			readpos += readlen;
			writepos += readlen;
			// decrement remaining count
			remaining -= readlen;
			// add to chksum
			for (loop = 0; loop < readlen; loop++) {
				chksum ^= buffer[loop];
			}
		}

#ifdef BOOT_IROM_CHKSUM
		if (sectcount == 0xff) {
			// just processed the irom section, now
			// read the normal header that follows
			if (SPIRead(readpos, header, sizeof(rom_header)) != 0) {
				return 0;
			}
			sectcount = header->count + 1;
			readpos += sizeof(rom_header);
		}
#endif
	}

	// round up to next 16 and get checksum
	readpos = readpos | 0x0f;
	if (SPIRead(readpos, buffer, 1) != 0) {
		return 0;
	}
	*totalSize = readpos - *totalSize + 5;
	// compare calculated and stored checksums
	INFO("ESPBOOT: stored ck: 0x%X, calc ck: 0x%X\r\n", buffer[0], chksum);
	if (buffer[0] != chksum) {
		return 0;
	}
	return romaddr;
}
void load_app(uint32 from_addr, uint32 to_addr, uint32 total_size)
{
	uint8 buffer[SECTOR_SIZE];
	uint32 readpos, writepos, sector = to_addr/SECTOR_SIZE;
	readpos = from_addr;
	writepos = to_addr;
	while(total_size > 0) {
		INFO(".");
		SPIEraseSector(sector);
		if (SPIRead(readpos, buffer, SECTOR_SIZE) != 0) {
			return;
		}
		//INFO("ESPBOOT: Write at 0x%X ...\r\n", writepos);
		if (SPIWrite(writepos, buffer, SECTOR_SIZE) != 0) {
			return;
		}
		writepos += SECTOR_SIZE;
		readpos += SECTOR_SIZE;
		if(total_size > SECTOR_SIZE)
			total_size -= SECTOR_SIZE;
		else
			total_size = 0;
		sector ++;
	}
	INFO(".\r\n");
}

void boot_app(uint32 addr)
{
	ramloader *loader;
	ets_memcpy((void*)_text_addr, _text_data, _text_len);
	if (addr != 0) {
		loader = (ramloader*)entry_addr;
		loader(addr);
	}
}


void save_boot_cfg(espboot_cfg *cfg)
{
	cfg->chksum = calc_chksum((uint8*)cfg, (uint8*)&cfg->chksum);
	if (SPIEraseSector(BOOT_CONFIG_SECTOR) != 0)
	{
		ERROR("Can not erase boot configuration sector\r\n");
	}
	if (SPIWrite(BOOT_CONFIG_SECTOR * SECTOR_SIZE, cfg, sizeof(espboot_cfg)) != 0)
	{
			ERROR("Can not save boot configurations\r\n");
	}
}

void load_boot_cfg(espboot_cfg *cfg)
{
	if (SPIRead(BOOT_CONFIG_SECTOR * SECTOR_SIZE, cfg, sizeof(espboot_cfg)) != 0)
	{
			ERROR("Can not read boot configurations\r\n");
	}
	INFO("ESPBOOT: Load");
	if(cfg->magic != BOOT_CONFIG_MAGIC || cfg->chksum != calc_chksum((uint8*)cfg, (uint8*)&cfg->chksum))
	{
		INFO(" default");
		cfg->magic = BOOT_CONFIG_MAGIC;
		cfg->app_rom_addr = DEFAULT_APPROM_ADDR;
		cfg->backup_rom_addr = DEFAULT_BACKUPROM_ADDR;
		cfg->new_rom_addr = 0x200000;
		save_boot_cfg(cfg);
	}
	INFO(" boot settings\r\n");
}

uint32 load(uint32 from_addr, uint32 to_addr)
{
	uint8 tryCount;
	uint32 total_size, romaddr;
	if(from_addr == 0)
		return 0;
	INFO("ESPBOOT: new application at 0x%X\r\n", from_addr);
	if(check_image(from_addr, &total_size) > 0)
	{
		INFO("ESPBOOT: checksum ok, try 3 times to load new app\r\n");
		tryCount = 3;
		while(tryCount --)
		{
			load_app(from_addr, to_addr, total_size);
			if((romaddr = check_image(to_addr, &total_size)) > 0)
			{
				return romaddr;
			}
			INFO("ESPBOOT: try load again [%d]\r\n", tryCount);
		}

		INFO("ESPBOOT: Can't load firmware\r\n");
		return 0;
	}
	INFO("ESPBOOT: invalid firmware\r\n");
	return 0;
}


void call_user_start()
{

	uint32 romaddr = 0, total_size = 0, boot_try = 3;
	INFO("ESPBOOT - New style Bootloader for ESP8266\r\n"
			 "Version: v0.1 - Release: 2015-Nov-01\r\n"
			 "Author: Tuan PM\r\n");

	load_boot_cfg(&boot_cfg);

	if(boot_cfg.new_rom_addr > 0)
	{
		if((romaddr = load(boot_cfg.new_rom_addr, boot_cfg.app_rom_addr)) > 0)
		{
			boot_cfg.new_rom_addr = 0;
			save_boot_cfg(&boot_cfg);
			INFO("ESPBOOT: everyting is ok, goto new app\r\n");
			boot_app(romaddr);
		}

		INFO("ESPBOOT: backup application at 0x%X\r\n", boot_cfg.backup_rom_addr);
		if((romaddr = load(boot_cfg.backup_rom_addr, boot_cfg.app_rom_addr)) > 0)
		{
			boot_cfg.new_rom_addr = 0;
			save_boot_cfg(&boot_cfg);
			INFO("ESPBOOT: everyting is ok, goto backup app\r\n");
			boot_app(romaddr);
		}
		INFO("ESPBOOT: Goto old application\r\n");
		boot_try = 3;
		while(boot_try-- > 0)
		{
			INFO("ESPBOOT: try boot application %d\r\n");
			if((romaddr = check_image(boot_cfg.app_rom_addr, &total_size)) > 0)
				boot_app(romaddr);
		}
		ERROR("ESPBOOT: Serious exception !!!\r\n");
	}

	while(boot_try-- > 0)
	{
		INFO("ESPBOOT: try boot application %d\r\n");
		if((romaddr = check_image(boot_cfg.app_rom_addr, &total_size)) > 0)
			boot_app(romaddr);
	}


	INFO("ESPBOOT: backup application at 0x%X\r\n", boot_cfg.backup_rom_addr);
	if((romaddr = load(boot_cfg.backup_rom_addr, boot_cfg.app_rom_addr)) > 0)
	{
		INFO("ESPBOOT: everyting is ok, goto backup app\r\n");
		boot_app(romaddr);
	}
	ERROR("Serious exception !!!");

}
