/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __RTW_S9082_EFUSE_H__
#define __RTW_S9082_EFUSE_H__
#include "rtw_s9082_fw.h"
#include "rtw_s9082.h"

/* Read-only efuse access: never enable the programming-voltage path. */
static inline int s9082_efuse_byte(const struct s9082_fw_io *io,
	unsigned int address, unsigned char *value)
{
	unsigned char control;
	unsigned int raw, count;
	if (address >= 256 ||
	    !io->write8(io->context, 0x4031, s9082_reverse_byte_bits(address & 255)) ||
	    !io->read8(io->context, 0x4032, &control) ||
	    !io->write8(io->context, 0x4032, control & 0x3f) ||
	    !io->read8(io->context, 0x4033, &control) ||
	    !io->write8(io->context, 0x4033, control & 0xfe))
		return 0;
	for (count = 0; count < 10000; count++) {
		if (io->stopped(io->context) || !io->read32(io->context, 0x4030, &raw))
			return 0;
		if (raw & 0x01000000U) {
			*value = raw & 255;
			return 1;
		}
	}
	return 0;
}

static inline int s9082_efuse_decode(const unsigned char *physical,
	unsigned int size, unsigned char logical[512])
{
	unsigned int cursor = 0, i;
	for (i = 0; i < 512; i++) logical[i] = 255;
	while (cursor < size) {
		unsigned int header = physical[cursor++], section, words;
		if (header == 255) return 1;
		if ((header & 31) == 15) {
			unsigned int extended;
			if (cursor >= size) return 0;
			extended = physical[cursor++];
			if ((extended & 15) == 15) continue;
			section = (header >> 5) | ((extended & 0xf0) >> 1);
			words = extended & 15;
		} else {
			section = header >> 4;
			words = header & 15;
		}
		for (i = 0; i < 4; i++) {
			if (!(words & (1U << i))) {
				if (cursor + 2 > size) return 0;
				if (section < 64) {
					logical[section * 8 + i * 2] = physical[cursor];
					logical[section * 8 + i * 2 + 1] = physical[cursor + 1];
				}
				cursor += 2;
			}
		}
	}
	return 1;
}
#endif
