/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __RTW_S9082_H__
#define __RTW_S9082_H__

/* Stock _to_ftaddr__cvrt: see docs/provenance.md and porting-progress.md. */
#define S9082_LOCAL_DEVICE_ID 1
#define S9082_TX_HIQ_DEVICE_ID 6
#define S9082_TX_MIQ_DEVICE_ID 4
#define S9082_TX_LOQ_DEVICE_ID 5

static inline unsigned int s9082_cmd_addr(unsigned int addr,
	unsigned char *device, unsigned short *offset)
{
	unsigned char id;
	unsigned short mask;

	switch (addr >> 16) {
	case 0x1025:
		id = S9082_LOCAL_DEVICE_ID;
		mask = 0x0fff;
		break;
	case 0x1031:
		id = S9082_TX_HIQ_DEVICE_ID;
		mask = 0x1fff;
		break;
	case 0x1032:
		id = S9082_TX_MIQ_DEVICE_ID;
		mask = 0x1fff;
		break;
	case 0x1033:
		id = S9082_TX_LOQ_DEVICE_ID;
		mask = 0x1fff;
		break;
	case 0x1034:
		id = 7;
		mask = 3;
		break;
	default:
		id = 8;
		mask = 0x7fff;
		break;
	}
	if (device)
		*device = id;
	if (offset)
		*offset = addr & mask;
	return ((unsigned int)id << 13) | (addr & mask);
}

/* Local IRQ/queue status reverses bits inside nibbles, not whole bytes. */
static inline unsigned int s9082_reverse_nibble_bits(unsigned int value)
{
	value = ((value & 0x55555555U) << 1) | ((value & 0xaaaaaaaaU) >> 1);
	return ((value & 0x33333333U) << 2) | ((value & 0xccccccccU) >> 2);
}

/* Transformed BB and mailbox paths reverse bits inside each byte. */
static inline unsigned int s9082_reverse_byte_bits(unsigned int value)
{
	value = s9082_reverse_nibble_bits(value);
	return ((value & 0x0f0f0f0fU) << 4) | ((value & 0xf0f0f0f0U) >> 4);
}

#endif
