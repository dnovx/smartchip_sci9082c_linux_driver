// SPDX-License-Identifier: GPL-2.0
/* Staged hardware experiment: no netdev or IRQ handler; always shuts down. */
#include <linux/module.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/unaligned.h>
#include "rtw_s9082.h"
#include "rtw_s9082_power.h"
#include "rtw_s9082_fw.h"
#include "rtw_s9082_init.h"
#include "rtw_s9082_efuse.h"
#include "../hal/rtl8188e/sdio/s9082_data.h"

static unsigned int stage;
module_param(stage, uint, 0444);
MODULE_PARM_DESC(stage, "0=read, 1=power, 2=firmware, 3=mailbox, 4=MAC/PHY/AGC, 5=RF table, 6=read efuse, 7=RF roundtrip, 8=passive RX; always shutdown");

struct diagnostic {
	struct sdio_func *func;
	unsigned long deadline;
	bool powered;
	int io_error;
	unsigned int reads, writes;
};

static int rd8(void *p, unsigned int addr, unsigned char *value)
{
	struct diagnostic *d = p;
	int err = 0;
	*value = sdio_readb(d->func, s9082_cmd_addr(addr, NULL, NULL), &err);
	d->reads++;
	if (err) d->io_error = err;
	return !err;
}
static int wr8(void *p, unsigned int addr, unsigned char value)
{
	struct diagnostic *d = p;
	int err = 0;
	sdio_writeb(d->func, value, s9082_cmd_addr(addr, NULL, NULL), &err);
	d->writes++;
	if (err) d->io_error = err;
	return !err;
}
static int rd32(void *p, unsigned int addr, unsigned int *value)
{
	struct diagnostic *d = p;
	int err = 0;
	if (addr >= 0x4000 && addr < 0x4100) {
		u8 bytes[4];
		unsigned int i;
		for (i = 0; i < 4; i++)
			if (!rd8(p, addr + i, bytes + i)) return 0;
		*value = get_unaligned_le32(bytes);
		return 1;
	}
	*value = sdio_readl(d->func, s9082_cmd_addr(addr, NULL, NULL), &err);
	d->reads++;
	if (err) d->io_error = err;
	return !err;
}
static int wr32(void *p, unsigned int addr, unsigned int value)
{
	struct diagnostic *d = p;
	int err = 0;
	if (addr >= 0x4000 && addr < 0x4100) {
		unsigned int i;
		for (i = 0; i < 4; i++)
			if (!wr8(p, addr + i, value >> (8 * i))) return 0;
		return 1;
	}
	sdio_writel(d->func, value, s9082_cmd_addr(addr, NULL, NULL), &err);
	d->writes++;
	if (err) d->io_error = err;
	return !err;
}
static int p_read(void *p, unsigned int addr, int local, unsigned char *value)
{
	return rd8(p, local ? 0x10250000 | (addr & 0xfff) : addr, value);
}
static int p_write(void *p, unsigned int addr, int local, unsigned char value)
{
	return wr8(p, local ? 0x10250000 | (addr & 0xfff) : addr, value);
}
static void delay_us(void *p, unsigned int us)
{
	if (us >= 1000) usleep_range(us, us + 100);
	else udelay(us);
}
static int sequence(struct diagnostic *d, const char *name, const unsigned char (*table)[8], unsigned int count)
{
	struct s9082_power_io io = {d, p_read, p_write, delay_us};
	unsigned int index;
	int result = s9082_power_sequence(&io, table, count, 255, 15, 1, &index);
	dev_info(&d->func->dev, "S9082-DIAG sequence=%s result=%d record=%u io_error=%d\n", name, result, index, d->io_error);
	return result;
}
#define SEQUENCE(d, t) sequence(d, #t, t, ARRAY_SIZE(t))

static int reset(void *p)
{
	u8 v, cpu;
#define R(a, v) do { if (!rd8(p, a, &v)) return 0; } while (0)
#define W(a, v) do { if (!wr8(p, a, v)) return 0; } while (0)
	R(0x401c, v); W(0x401c, v & 0xbf);
	R(0x401d, v); W(0x401d, v & 0xef);
	R(0x4003, cpu); W(0x4003, cpu & 0xdf);
	R(0x401c, v); W(0x401c, v & 0xbf);
	R(0x401d, v); W(0x401d, v | 0x10);
	W(0x4003, cpu | 0x20);
#undef R
#undef W
	return 1;
}
static unsigned int now(void *p) { return jiffies_to_msecs(jiffies); }
static int stopped(void *p)
{
	struct diagnostic *d = p;
	return d->io_error || time_after(jiffies, d->deadline);
}
static void wait_ms(void *p) { usleep_range(1000, 1500); }

static int power_on(struct diagnostic *d)
{
	u8 bytes[2];
	if (!wr8(d, 0x401c, 0)) return 0;
	d->powered = true; /* A partial enable still requires a teardown attempt. */
	if (!SEQUENCE(d, s9082_Tag_en_run)) return 0;
	if (!rd8(d, 0x4004, bytes) || !rd8(d, 0x4005, bytes + 1)) return 0;
	if (!wr8(d, 0x4004, bytes[0] | 8) || !wr8(d, 0x4005, bytes[1])) return 0;
	if (!rd8(d, 0x4100, bytes) || !rd8(d, 0x4101, bytes + 1)) return 0;
	return wr8(d, 0x4100, bytes[0] | 0xfc) && wr8(d, 0x4101, bytes[1] | 0x60);
}
static int power_check(struct diagnostic *d)
{
	u8 bytes[4];
	u32 value;
	int i;
	for (i = 0; i < 4; i++)
		if (!rd8(d, 0x4100 + i, bytes + i) || bytes[i] == 0x57) return 0;
	if (!rd32(d, 0x4100, &value) || value != get_unaligned_le32(bytes)) return 0;
	return wr32(d, 0x41b8, 0x12345678) && rd32(d, 0x41b8, &value) && value == 0x12345678;
}
static void power_off(struct diagnostic *d, bool fw_ready)
{
	u8 v;
	bool ok = true;
#define UPDATE(a, clear, set) do { if (rd8(d, a, &v)) ok &= wr8(d, a, (v & (clear)) | (set)); else ok = false; } while (0)
	UPDATE(0x44ec, 0xbf, 0);
	ok &= wr8(d, 0x4100, 0);
	if (rd8(d, 0x4044, &v)) ok &= wr8(d, 0x4045, v); else ok = false;
	ok &= wr8(d, 0x4046, 0xff);
	if (rd8(d, 0x4042, &v)) ok &= wr8(d, 0x4042, v | (v >> 4)); else ok = false;
	UPDATE(0x4043, 0xff, 0xf0);
	ok &= SEQUENCE(d, s9082_enter_lps_run);
	if (rd8(d, 0x4080, &v) && (v & 1) && fw_ready) { UPDATE(0x4003, 0xdf, 0); }
	ok &= wr8(d, 0x4080, 0);
	ok &= SEQUENCE(d, s9082_Tag_close_run);
	ok &= wr8(d, 0x401c, 0x70);
	dev_info(&d->func->dev, "S9082-DIAG shutdown result=%d io_error=%d\n", ok, d->io_error);
#undef UPDATE
}

static int mailbox_check(struct diagnostic *d)
{
	/* Stock power-mode ACTIVE: mode 0, SmartPS 0, awake interval 2,
	 * allQueueUAPSD 0, power state 0x0c. This requests no RF transmission. */
	static const u8 payload[] = {0, 0, 2, 0, 0x0c};
	unsigned int box, i, retry;
	u8 status;
	for (box = 0; box < 4; box++) {
		for (retry = 0; retry < 101; retry++) {
			if (!rd8(d, 0x41cc, &status)) return 0;
			if (!(status & BIT(7 - box))) break;
			wait_ms(d);
		}
		if (retry == 101) return 0;
		for (i = 3; i < sizeof(payload); i++)
			if (!wr8(d, 0x41f0 + box * 4 + i - 3, s9082_reverse_byte_bits(payload[i]))) return 0;
		for (i = 0; i < 4; i++)
			if (!wr8(d, 0x41d0 + box * 4 + i, s9082_reverse_byte_bits(i ? payload[i - 1] : 0x20))) return 0;
		/* Require eventual consumption; observing a busy transition is separate. */
		for (retry = 0; retry < 101; retry++) {
			if (!rd8(d, 0x41cc, &status)) return 0;
			if (!(status & BIT(7 - box))) break;
			wait_ms(d);
		}
		dev_info(&d->func->dev, "S9082-DIAG mailbox box=%u clear=%d polls=%u status=%#x\n", box, retry < 101, retry + 1, status);
		if (retry == 101) return 0;
	}
	return 1;
}

static int wr16(struct diagnostic *d, unsigned int addr, u16 value)
{
	int err = 0;
	if (addr >= 0x4000 && addr < 0x4100)
		return wr8(d, addr, value & 255) && wr8(d, addr + 1, value >> 8);
	sdio_writew(d->func, value, s9082_cmd_addr(addr, NULL, NULL), &err);
	d->writes++;
	if (err) d->io_error = err;
	return !err;
}

static int init_ops(struct diagnostic *d, const char *name, const struct s9082_init_op *ops, unsigned int count)
{
	unsigned int i, raw;
	for (i = 0; i < count; i++) {
		const struct s9082_init_op *op = ops + i;
		if (stopped(d)) return 0;
		switch (op->kind) {
		case 1: if (!wr8(d, op->address, op->value)) return 0; break;
		case 2: if (!wr16(d, op->address, op->value)) return 0; break;
		case 4: if (!wr32(d, op->address, op->value)) return 0; break;
		case 5:
		case 6:
			raw = 0;
			if (op->mask != 0xffffffffU && !rd32(d, op->address, &raw)) return 0;
			if (!wr32(d, op->address, s9082_bb_merge(raw, op->mask, op->value, op->kind == 5))) return 0;
			break;
		case 7: delay_us(d, op->value); break;
		case 8: delay_us(d, op->value * 1000); break;
		case 9: msleep(op->value); break;
		default: return 0;
		}
	}
	dev_info(&d->func->dev, "S9082-DIAG init=%s operations=%u result=1\n", name, count);
	return 1;
}

static int baseband_check(struct diagnostic *d)
{
	u8 low, high;
	u32 control, agc;
	if (!init_ops(d, "MAC", s9082_init_mac, ARRAY_SIZE(s9082_init_mac)) ||
	    !wr16(d, 0x44ca, 0xe0e0)) return 0;
	/* ConfigPHY_BB9083X hardware prerequisites; PHY-PG/state remains separate. */
	if (!rd8(d, 0x4002, &low) || !rd8(d, 0x4003, &high) ||
	    !wr16(d, 0x4002, (low | (high << 8)) | 0x4c0) ||
	    !wr8(d, 0x401f, 0xe0) || !wr8(d, 0x4002, 0xc7)) return 0;
	if (!init_ops(d, "PHY", s9082_init_phy, ARRAY_SIZE(s9082_init_phy)) ||
	    !init_ops(d, "AGC", s9082_init_agc, ARRAY_SIZE(s9082_init_agc))) return 0;
	if (!rd32(d, 0x4800, &control) || !rd32(d, 0x4c78, &agc)) return 0;
	dev_info(&d->func->dev, "S9082-DIAG BB raw4800=%#x raw4c78=%#x RF-init=skipped\n", control, agc);
	return 1;
}

static int bb_field(struct diagnostic *d, unsigned int address, unsigned int mask,
	unsigned int value, bool transformed)
{
	u32 raw;
	return rd32(d, address, &raw) && wr32(d, address, s9082_bb_merge(raw, mask, value, transformed));
}
static int rf_check(struct diagnostic *d)
{
	u32 saved;
	int result;
	if (!rd32(d, 0x4870, &saved)) return 0;
	saved = (s9082_reverse_byte_bits(saved) >> 4) & 1;
	if (!bb_field(d, 0x4860, 0x100000, 1, true)) return 0;
	delay_us(d, 1);
	if (!bb_field(d, 0x4860, 0x10, 1, true)) return 0;
	delay_us(d, 1);
	if (!bb_field(d, 0x4824, 0x400, 0, false)) return 0;
	delay_us(d, 1);
	if (!bb_field(d, 0x4824, 0x800, 0, false)) return 0;
	delay_us(d, 1);
	result = init_ops(d, "RF", s9082_init_rf, ARRAY_SIZE(s9082_init_rf));
	if (!bb_field(d, 0x4870, 0x10, saved, true)) return 0;
	return result;
}

static int efuse_check(struct diagnostic *d, const struct s9082_fw_io *io)
{
	u8 physical[256], logical[512], low, high;
	unsigned int i;
	int result = 0;
	if (!wr8(d, 0x40cf, 0x96) || !rd8(d, 0x4002, &low) || !rd8(d, 0x4003, &high) ||
	    !wr16(d, 0x4002, (low | (high << 8)) | 0x800) ||
	    !rd8(d, 0x4008, &low) || !rd8(d, 0x4009, &high) ||
	    !wr16(d, 0x4008, (low | (high << 8)) | 0x44)) goto finish;
	for (i = 0; i < 256; i++)
		if (!s9082_efuse_byte(io, i, physical + i)) {
			u32 raw = 0;
			rd32(d, 0x4030, &raw);
			dev_info(&d->func->dev, "S9082-DIAG efuse timeout address=%u control=%#x\n", i, raw);
			goto finish;
		}
	result = s9082_efuse_decode(physical, 256, logical);
	dev_info(&d->func->dev, "S9082-DIAG efuse decode=%d\n", result);
	for (i = 0; i < 256; i += 16)
		dev_info(&d->func->dev, "S9082-EFUSE-PHYS %03x %*phN\n", i, 16, physical + i);
	for (i = 0; i < 512; i += 16)
		dev_info(&d->func->dev, "S9082-EFUSE-MAP %03x %*phN\n", i, 16, logical + i);
finish:
	if (!wr8(d, 0x40cf, 0)) result = 0;
	return result;
}

static int rf_readback(struct diagnostic *d)
{
	static const unsigned int registers[] = {0, 0x18, 0x1f};
	unsigned int i, saved, select, value, original_channel = 0;
	for (i = 0; i < ARRAY_SIZE(registers); i++) {
		if (!rd32(d, 0x4824, &saved) || !wr32(d, 0x4824, saved & 0x7fffffff)) return 0;
		delay_us(d, 10);
		if (!wr32(d, 0x4824, (registers[i] << 23) | 0x80000000 | (saved & 0x807fffff))) return 0;
		delay_us(d, 110);
		if (!rd32(d, 0x4820, &select) ||
		    !rd32(d, (s9082_reverse_byte_bits(select) & 0x100) ? 0x48b8 : 0x48a0, &value)) return 0;
		dev_info(&d->func->dev, "S9082-DIAG RF read reg=%#x value=%#x raw=%#x\n",
			registers[i], s9082_reverse_byte_bits(value) & 0xfffff, value);
		if (registers[i] == 0x18) original_channel = s9082_reverse_byte_bits(value) & 0xfffff;
	}
	/* Change only channel bits, verify the serial write, then restore exactly. */
	for (i = 0; i < 2; i++) {
		unsigned int wanted = i ? original_channel : (original_channel & ~0x3ffU) | 1;
		if (!wr32(d, 0x4840, s9082_reverse_byte_bits((0x18U << 20) | wanted))) return 0;
		delay_us(d, 1000);
		if (!rd32(d, 0x4824, &saved) || !wr32(d, 0x4824, saved & 0x7fffffff)) return 0;
		delay_us(d, 10);
		if (!wr32(d, 0x4824, (0x18U << 23) | 0x80000000 | (saved & 0x807fffff))) return 0;
		delay_us(d, 110);
		if (!rd32(d, 0x4820, &select) ||
		    !rd32(d, (s9082_reverse_byte_bits(select) & 0x100) ? 0x48b8 : 0x48a0, &value)) return 0;
		value = s9082_reverse_byte_bits(value) & 0xfffff;
		dev_info(&d->func->dev, "S9082-DIAG RF channel roundtrip wanted=%#x actual=%#x restore=%u\n", wanted, value, i);
		if (wanted != value) return 0;
	}
	return 1;
}

static int llt_write(struct diagnostic *d, unsigned int address, unsigned int next)
{
	unsigned int retry, raw;
	if (!wr32(d, 0x41e0, s9082_reverse_byte_bits(0x40000000U | (address << 8) | next))) return 0;
	for (retry = 0; retry < 20; retry++) {
		if (!rd32(d, 0x41e0, &raw)) return 0;
		if (!(raw & 0x03000000)) return 1;
	}
	return 0;
}

static int receive_check(struct diagnostic *d)
{
	unsigned int i, raw, length, count = 0;
	u8 buffer[512];
	u8 *rx_buffer;
	int error;
	/* Fixed normal-mode queue allocation and boundary from stock hal_init. */
	if (!wr8(d, 0x4214, s9082_reverse_byte_bits(0)) ||
	    !wr32(d, 0x4200, s9082_reverse_byte_bits(0x80ee0900)) ||
	    !wr16(d, 0x4116, 0xbcff) || !wr8(d, 0x4104, 0x88) ||
	    !wr8(d, 0x4424, 0x1f) || !wr8(d, 0x4425, 0x1f) ||
	    !wr8(d, 0x445d, 0x1f) || !wr8(d, 0x4114, 0x1f) ||
	    !wr8(d, 0x4209, 0xf8)) return 0;
	for (i = 0; i < 0xf7; i++) if (!llt_write(d, i, i + 1)) return 0;
	if (!llt_write(d, 0xf7, 255) || !llt_write(d, 255, 0xf8)) return 0;
	for (i = 0xf8; i < 255; i++) if (!llt_write(d, i, i + 1)) return 0;
	if (!wr8(d, 0x460f, 0x20) || !wr32(d, 0x4608, s9082_reverse_byte_bits(0x7000600f)) ||
	    !wr16(d, 0x46a4, 0xffff) || !wr16(d, 0x46a2, 0x2000) ||
	    !wr16(d, 0x46a0, 0xffff) || !wr8(d, 0x4281, 0x60) || !wr8(d, 0x4280, 0x24) ||
	    !rd8(d, 0x410c, buffer) || !wr8(d, 0x410c, buffer[0] | 0x20) ||
	    !bb_field(d, 0x4800, 0x03000000, 3, true) ||
	    !rd8(d, 0x4100, buffer) || !wr8(d, 0x4100, buffer[0] | 3)) return 0;
	/* Passive polling only: no interrupt registration or transmitted probes. */
	for (i = 0; i < 5000; i++) {
		if (!(i % 350)) {
			unsigned int channel = (i / 350) % 13 + 1;
			if (!wr32(d, 0x4840, s9082_reverse_byte_bits((0x18U << 20) | 0x7c00 | channel))) return 0;
			dev_info(&d->func->dev, "S9082-DIAG passive channel=%u\n", channel);
		}
		if (!rd32(d, 0x10252018, &raw)) return 0;
		if (s9082_reverse_nibble_bits(raw) & 1) {
			if (!rd32(d, 0x1025201c, &length)) return 0;
			length = s9082_reverse_nibble_bits(length) & 0xffff;
			dev_info(&d->func->dev, "S9082-DIAG RX pending length=%u status=%#x\n", length, raw);
			if (!length || length > sizeof(buffer)) break;
			rx_buffer = kmalloc(ALIGN(length, 4), GFP_KERNEL);
			if (!rx_buffer) return 0;
			error = sdio_memcpy_fromio(d->func, rx_buffer, 0xe000 | (count & 3), ALIGN(length, 4));
			if (!error) memcpy(buffer, rx_buffer, length);
			kfree(rx_buffer);
			if (error) { d->io_error = error; return 0; }
			dev_info(&d->func->dev, "S9082-DIAG RX prefix %*phN\n", min(length, 64U), buffer);
			count++;
		}
		usleep_range(1000, 1500);
	}
	dev_info(&d->func->dev, "S9082-DIAG RX packets=%u\n", count);
	return 1;
}

static int probe(struct sdio_func *func, const struct sdio_device_id *id)
{
	struct diagnostic d = {.func = func, .deadline = jiffies + 15 * HZ};
	struct s9082_fw_io fw = {&d, rd8, wr8, rd32, wr32, reset, now, stopped, wait_ms};
	int err;
	bool result = false, ready = false;
	u8 control = 0, local = 0;
	if (stage > 8) return -EINVAL;
	sdio_claim_host(func);
	err = sdio_enable_func(func);
	if (err) goto release;
	err = sdio_set_block_size(func, 512);
	if (err) goto disable;
	dev_info(&func->dev, "S9082-DIAG begin stage=%u\n", stage);
	result = rd8(&d, 0x4080, &control) && rd8(&d, 0x10250086, &local);
	dev_info(&func->dev, "S9082-DIAG initial result=%d fwctrl=%#x local86=%#x\n", result, control, local);
	if (stage && result) {
		result = power_on(&d) && power_check(&d);
		dev_info(&func->dev, "S9082-DIAG power-check result=%d\n", result);
	}
	if (stage >= 2 && stage != 6 && result) {
		ready = s9082_fw_download(&fw, s9082_nic_firmware, sizeof(s9082_nic_firmware));
		result = ready;
		rd8(&d, 0x4080, &control);
		dev_info(&func->dev, "S9082-DIAG firmware-ready result=%d fwctrl=%#x\n", ready, control);
	}
	if (stage >= 3 && stage != 6 && result) {
		result = mailbox_check(&d);
		dev_info(&func->dev, "S9082-DIAG mailbox result=%d\n", result);
	}
	if (stage >= 4 && stage != 6 && result) {
		result = baseband_check(&d);
		dev_info(&func->dev, "S9082-DIAG baseband result=%d\n", result);
	}
	if ((stage == 5 || stage >= 7) && result) {
		result = rf_check(&d);
		dev_info(&func->dev, "S9082-DIAG RF table result=%d (I/O completion only)\n", result);
	}
	if (stage == 7 && result) result = rf_readback(&d);
	if (stage == 8 && result) result = receive_check(&d);
	if (stage == 6 && result) {
		result = efuse_check(&d, &fw);
		dev_info(&func->dev, "S9082-DIAG efuse result=%d\n", result);
	}
	if (d.powered) power_off(&d, ready);
	dev_info(&func->dev, "S9082-DIAG end stage=%u result=%d reads=%u writes=%u io_error=%d\n", stage, result, d.reads, d.writes, d.io_error);
disable:
	{
		int cleanup = sdio_disable_func(func);
		if (cleanup) dev_err(&func->dev, "S9082-DIAG disable failed=%d\n", cleanup);
	}
release:
	sdio_release_host(func);
	/* Never retain a binding or register a network interface after the experiment. */
	return err ? err : -ENODEV;
}
static void remove_device(struct sdio_func *func) { }
static const struct sdio_device_id ids[] = {{SDIO_DEVICE(0x02e7, 0x9082)}, {}};
MODULE_DEVICE_TABLE(sdio, ids);
static struct sdio_driver s9082_driver = {.name = "s9082_diag", .id_table = ids, .probe = probe, .remove = remove_device};
module_sdio_driver(s9082_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Restricted S9082 SDIO power and firmware diagnostic");
