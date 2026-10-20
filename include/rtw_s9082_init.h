/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __RTW_S9082_INIT_H__
#define __RTW_S9082_INIT_H__
#include "rtw_s9082.h"
struct s9082_init_op {
	unsigned int kind, address, mask, value;
};

/* Mask values are in logical bit order even on byte-transformed BB paths. */
static inline unsigned int s9082_bb_merge(unsigned int raw, unsigned int mask,
	unsigned int value, int transformed)
{
	unsigned int shift = 0, logical = transformed ? s9082_reverse_byte_bits(raw) : raw;
	if (mask != 0xffffffffU) {
		if (!mask)
			return raw;
		while (!(mask & (1U << shift)))
			shift++;
		value = (logical & ~mask) | ((value << shift) & mask);
	}
	return transformed ? s9082_reverse_byte_bits(value) : value;
}
#endif
