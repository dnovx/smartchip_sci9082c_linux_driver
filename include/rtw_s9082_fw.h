/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __RTW_S9082_FW_H__
#define __RTW_S9082_FW_H__
struct s9082_fw_io {
	void *context;
	int (*read8)(void *, unsigned int, unsigned char *);
	int (*write8)(void *, unsigned int, unsigned char);
	int (*read32)(void *, unsigned int, unsigned int *);
	int (*write32)(void *, unsigned int, unsigned int);
	int (*reset)(void *);
	unsigned int (*time_ms)(void *);
	int (*stopped)(void *);
	void (*wait)(void *);
};

/* Reverse the three page-index bits into bits 7:5; retain bits 4:0. */
static inline unsigned char s9082_fw_page(unsigned char control, unsigned int page)
{
	return (control & 0x1f) | ((page & 1) << 7) |
		((page & 2) << 5) | ((page & 4) << 3);
}

static inline int s9082_fw_write_page(const struct s9082_fw_io *io,
	unsigned int page, const unsigned char *bytes, unsigned int length)
{
	unsigned int i;
	unsigned char control;
	if (page > 7 || length > 4096 || !length ||
	    !io->read8(io->context, 0x4082, &control) ||
	    !io->write8(io->context, 0x4082, s9082_fw_page(control, page)))
		return 0;
	for (i = 0; i + 4 <= length; i += 4) {
		unsigned int word = bytes[i] | ((unsigned int)bytes[i+1] << 8) |
			((unsigned int)bytes[i+2] << 16) | ((unsigned int)bytes[i+3] << 24);
		if (io->stopped(io->context) || !io->write32(io->context, 0x5000 + i, word))
			return 0;
	}
	for (; i < length; i++)
		if (!io->write8(io->context, 0x5000 + i, bytes[i]))
			return 0;
	return 1;
}

static inline int s9082_fw_poll(const struct s9082_fw_io *io,
	unsigned int bit, unsigned int timeout, unsigned int minimum)
{
	unsigned int start = io->time_ms(io->context), count = 0, value;
	do {
		if (io->stopped(io->context) || !io->read32(io->context, 0x4080, &value))
			return 0;
		count++;
		if (value & bit)
			return 1;
		io->wait(io->context);
	} while (io->time_ms(io->context) - start < timeout || count < minimum);
	return 0;
}

static inline int s9082_fw_download(const struct s9082_fw_io *io,
	const unsigned char *image, unsigned int length)
{
	unsigned char value;
	unsigned int word, start, attempt = 0, offset;
	int success = 0, disabled;
	if (!image || !length || length > 32768 || io->stopped(io->context))
		return 0;
	if (!io->read8(io->context, 0x4080, &value))
		return 0;
	if ((value & 1) && (!io->write8(io->context, 0x4080, 0) || !io->reset(io->context)))
		return 0;
	if (!io->read8(io->context, 0x4080, &value) ||
	    !io->write8(io->context, 0x4080, value | 0x80))
		goto disable;
	if (!io->read8(io->context, 0x4082, &value) ||
	    !io->write8(io->context, 0x4082, value & 0xef))
		goto disable;
	start = io->time_ms(io->context);
	do {
		if (io->stopped(io->context))
			break;
		if (!io->read8(io->context, 0x4080, &value) ||
		    !io->write8(io->context, 0x4080, value | 0x20))
			break;
		for (offset = 0; offset < length; offset += 4096) {
			unsigned int chunk = length - offset;
			if (chunk > 4096)
				chunk = 4096;
			if (!s9082_fw_write_page(io, offset / 4096, image + offset, chunk))
				break;
		}
		if (offset >= length && s9082_fw_poll(io, 0x20, 50, 5)) {
			success = 1;
			break;
		}
		attempt++;
		io->wait(io->context);
	} while (attempt < 3 || io->time_ms(io->context) - start < 500);
disable:
	disabled = io->read8(io->context, 0x4080, &value);
	if (disabled)
		disabled = io->write8(io->context, 0x4080, value & 0x7f);
	/* Always attempt both cleanup writes, including failed downloads. */
	if (!io->write8(io->context, 0x4081, 0) || !disabled || !success)
		return 0;
	if (!io->read32(io->context, 0x4080, &word) ||
	    !io->write32(io->context, 0x4080, (word & ~2U) | 0x40) ||
	    !io->reset(io->context))
		return 0;
	return s9082_fw_poll(io, 2, 200, 10);
}
#endif
