/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __RTW_S9082_POWER_H__
#define __RTW_S9082_POWER_H__

/* A small I/O boundary lets the actual stock-record interpreter run on a host. */
struct s9082_power_io {
	void *context;
	int (*read)(void *, unsigned int, int, unsigned char *);
	int (*write)(void *, unsigned int, int, unsigned char);
	void (*delay_us)(void *, unsigned int);
};

/* Returns 1 on selected END; 0 on timeout, I/O failure or malformed sequence. */
static inline int s9082_power_sequence(const struct s9082_power_io *io,
	const unsigned char records[][8], unsigned int count,
	unsigned char cut, unsigned char fab, unsigned char interface,
	unsigned int *failed_record)
{
	unsigned int i, polls = 0;
	for (i = 0; i < count; i++) {
		const unsigned char *r = records[i];
		unsigned int address = r[0] | ((unsigned int)r[1] << 8);
		unsigned int command = r[4] >> 4;
		int local = (r[4] & 15) == 3;
		unsigned char value;
		*failed_record = i;
		if (!(r[2] & cut) || !(r[3] & fab & 15) || !((r[3] >> 4) & interface))
			continue;
		switch (command) {
		case 0:
			break;
		case 1:
			if (!io->read(io->context, address, local, &value))
				return 0;
			value = (value & (unsigned char)~r[5]) | (r[6] & r[5]);
			if (!io->write(io->context, address, local, value))
				return 0;
			break;
		case 2:
			for (;;) {
				if (!io->read(io->context, address, local, &value))
					return 0;
				if ((value & r[5]) == (r[6] & r[5])) {
					if (polls++ > 5000)
						return 0;
					break;
				}
				io->delay_us(io->context, 10);
				if (polls++ > 5000)
					return 0;
			}
			break;
		case 3:
			io->delay_us(io->context, address * (r[6] ? 1000U : 1U));
			break;
		case 4:
			return 1;
		default:
			return 0;
		}
	}
	*failed_record = count;
	return 0;
}

#endif
