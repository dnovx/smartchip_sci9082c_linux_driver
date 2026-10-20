/* SPDX-License-Identifier: GPL-2.0 */
/* Independent address/encoding vectors and bounded lifecycle failure cases. */
#include <assert.h>
#include <stdio.h>
#include "rtw_s9082_init.h"
#include "rtw_s9082_power.h"
#include "rtw_s9082_fw.h"
#include "rtw_s9082_efuse.h"

static int read_zero(void *context, unsigned int address, int local, unsigned char *value)
{
	unsigned int *reads = context;
	(void)address; (void)local;
	(*reads)++;
	*value = 0;
	return 1;
}
static int write_fail(void *context, unsigned int address, int local, unsigned char value)
{
	(void)context; (void)address; (void)local; (void)value;
	return 0;
}
static void delay(void *context, unsigned int us) { (void)context; (void)us; }

int main(void)
{
	static const unsigned int vectors[][4] = {
		{0x10250000, 0x2000, 1, 0},
		{0x10252014, 0x2014, 1, 0x14},
		{0x1025ffff, 0x2fff, 1, 0xfff},
		{0x10310080, 0xc080, 6, 0x80},
		{0x10320080, 0x8080, 4, 0x80},
		{0x10330080, 0xa080, 5, 0x80},
		{0x1031ffff, 0xdfff, 6, 0x1fff},
		{0x1034ffff, 0xe003, 7, 3},
		{0x00004080, 0x14080, 8, 0x4080},
		{0xffffffff, 0x17fff, 8, 0x7fff},
	};
	const unsigned char timeout[][8] = {
		{0, 0, 255, 0x1f, 0x20, 1, 1, 0},
		{0, 0, 255, 0x1f, 0x40, 0, 0, 0},
	};
	const unsigned char write[][8] = {{0, 0, 255, 0x1f, 0x10, 255, 1, 0}};
	unsigned int i, j, lane, reads = 0, failed;
	struct s9082_power_io io = {&reads, read_zero, write_fail, delay};
	unsigned char logical[512], device;
	unsigned short offset;
	const unsigned char valid[] = {0x0e, 0x82, 0x90, 255};
	const unsigned char truncated[] = {0x0e, 0x82};
	for (i = 0; i < sizeof(vectors) / sizeof(vectors[0]); i++) {
		assert(s9082_cmd_addr(vectors[i][0], &device, &offset) == vectors[i][1]);
		assert(device == vectors[i][2] && offset == vectors[i][3]);
	}
	for (i = 0; i < 256; i++) {
		unsigned int byte = 0, nibble = 0;
		for (j = 0; j < 8; j++) {
			byte |= ((i >> j) & 1) << (7 - j);
			nibble |= ((i >> j) & 1) << ((j & 4) + 3 - (j & 3));
		}
		for (lane = 0; lane < 32; lane += 8) {
			assert(s9082_reverse_byte_bits(i << lane) == byte << lane);
			assert(s9082_reverse_nibble_bits(i << lane) == nibble << lane);
		}
	}
	assert(s9082_bb_merge(0x12345678, 0x00ff0000, 0xabcdef42, 0) == 0x12425678);
	assert(s9082_bb_merge(0x12345678, 0, 0xffffffffU, 1) == 0x12345678);
	for (i = 0; i < 8; i++) {
		static const unsigned char expected[] = {0, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0};
		assert(s9082_fw_page(0x1b, i) == (expected[i] | 0x1b));
	}
	assert(!s9082_power_sequence(&io, timeout, 2, 255, 15, 1, &failed));
	assert(reads == 5002 && failed == 0);
	assert(!s9082_power_sequence(&io, write, 1, 255, 15, 1, &failed));
	assert(s9082_efuse_decode(valid, sizeof(valid), logical));
	assert(logical[0] == 0x82 && logical[1] == 0x90 && logical[2] == 255);
	assert(!s9082_efuse_decode(truncated, sizeof(truncated), logical));
	puts("PASS: SDIO vectors, exhaustive encoding, masks, firmware pages, power timeout/I-O failure, efuse bounds");
	return 0;
}
