/*
 * ram_loader.c
 *
 *  Created on: Oct 25, 2015
 *      Author: TuanPM
 */
#include "espboot.h"
rom_header header;
rom_header_new header_new;

loader* NOINLINE load_rom(uint32 readpos) {

	uint8 buffer[BUFFER_SIZE];
	uint8 sectcount;
	uint8 *writepos;
	uint32 remaining;
	loader* usercode;

	rom_header *header = (rom_header*)buffer;
	section_header *section = (section_header*)buffer;

	// read rom header
	SPIRead(readpos, header, sizeof(rom_header));
	readpos += sizeof(rom_header);

	// create function pointer for entry point
	usercode = header->entry;

	// copy all the sections
	for (sectcount = header->count; sectcount > 0; sectcount--) {

		// read section header
		SPIRead(readpos, section, sizeof(section_header));
		readpos += sizeof(section_header);

		// get section address and length
		writepos = section->address;
		remaining = section->length;

		while (remaining > 0) {
			// work out how much to read, up to 16 bytes at a time
			uint32 readlen = (remaining < BUFFER_SIZE) ? remaining : BUFFER_SIZE;
			// read the block
			SPIRead(readpos, buffer, readlen);
			readpos += readlen;
			// copy the block
			ets_memcpy(writepos, buffer, readlen);
			// increment next write position
			writepos += readlen;
			// decrement remaining count
			remaining -= readlen;
		}
	}

	return usercode;
}
void call_user_start(uint32 readpos) {
	loader* load;
	load = load_rom(readpos);
	load();
}
