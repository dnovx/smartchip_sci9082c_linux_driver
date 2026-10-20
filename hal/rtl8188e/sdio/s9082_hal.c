/* SPDX-License-Identifier: GPL-2.0 */
#include <rtl8188e_hal.h>
#include <rtw_s9082_hal.h>
#include <rtw_s9082_power.h>
#include <rtw_s9082_fw.h>
#include <rtw_s9082_init.h>
#include <rtw_s9082_efuse.h>
#include "s9082_data.h"
#include <rtw_s9082_trace.h>

bool s9082_scan_trace;
bool s9082_ack_failfast = true;
module_param_named(s9082_ack_failfast, s9082_ack_failfast, bool, 0644);
MODULE_PARM_DESC(s9082_ack_failfast, "Fail unconfirmed management ACKs immediately (default enabled)");
atomic_t s9082_trace_remaining = ATOMIC_INIT(600);
module_param_named(s9082_scan_trace, s9082_scan_trace, bool, 0644);
MODULE_PARM_DESC(s9082_scan_trace, "Bounded SCI scan/TX-report timing diagnostics");

static int power_read(void *context, unsigned int address, int local, unsigned char *value)
{
	PADAPTER adapter = context;
	struct intf_hdl *intf = &adapter->iopriv.intf;
	u32 command = local ? (0x2000 | (address & 0xfff)) : s9082_cmd_addr(address, NULL, NULL);
	if (RTW_CANNOT_IO(adapter))
		return 0;
	return sd_cmd52_read(intf, command, 1, value) == 0;
}

static int power_write(void *context, unsigned int address, int local, unsigned char value)
{
	PADAPTER adapter = context;
	struct intf_hdl *intf = &adapter->iopriv.intf;
	u32 command = local ? (0x2000 | (address & 0xfff)) : s9082_cmd_addr(address, NULL, NULL);
	if (RTW_CANNOT_IO(adapter))
		return 0;
	return sd_cmd52_write(intf, command, 1, &value) == 0;
}

static void power_delay(void *context, unsigned int us)
{
	/* Avoid architecture udelay limits on millisecond-scale stock records. */
	while (us >= 1000) {
		rtw_udelay_os(1000);
		us -= 1000;
	}
	if (us)
		rtw_udelay_os(us);
}

static int run_power(PADAPTER adapter, const char *name,
	const unsigned char records[][8], unsigned int count)
{
	struct s9082_power_io io = {adapter, power_read, power_write, power_delay};
	unsigned int index;
	int result = s9082_power_sequence(&io, records, count, 0xff, 0xf, 1, &index);
	if (!result)
		RTW_ERR("S9082 power %s failed at record %u address %#x\n", name, index,
			index < count ? records[index][0] | (records[index][1] << 8) : 0);
	return result;
}

#define RUN_POWER(adapter, table) run_power(adapter, #table, table, ARRAY_SIZE(table))

u32 s9082_power_on(PADAPTER adapter)
{
	u8 on = _FALSE;
	rtw_hal_get_hwreg(adapter, HW_VAR_APFM_ON_MAC, &on);
	if (!on) {
		if (rtw_write8(adapter, 0x401c, 0) != _SUCCESS ||
		    !RUN_POWER(adapter, s9082_Tag_en_run))
			return _FAIL;
		on = _TRUE;
		rtw_hal_set_hwreg(adapter, HW_VAR_APFM_ON_MAC, &on);
	}
	if (rtw_write16(adapter, 0x4004, rtw_read16(adapter, 0x4004) | 8) != _SUCCESS ||
	    rtw_write16(adapter, 0x4100, rtw_read16(adapter, 0x4100) | 0x60fc) != _SUCCESS)
		return _FAIL;
	return _SUCCESS;
}

void s9082_power_off(PADAPTER adapter)
{
	u8 on = _FALSE, value;
	rtw_hal_get_hwreg(adapter, HW_VAR_APFM_ON_MAC, &on);
	if (!on)
		return;
	rtw_write8(adapter, 0x44ec, rtw_read8(adapter, 0x44ec) & 0xbf);
	rtw_write8(adapter, 0x4100, 0);
	rtw_write8(adapter, 0x4045, rtw_read8(adapter, 0x4044));
	rtw_write8(adapter, 0x4046, 0xff);
	value = rtw_read8(adapter, 0x4042);
	rtw_write8(adapter, 0x4042, value | (value >> 4));
	rtw_write8(adapter, 0x4043, rtw_read8(adapter, 0x4043) | 0xf0);
	RUN_POWER(adapter, s9082_enter_lps_run);
	if ((rtw_read8(adapter, 0x4080) & 1) && GET_HAL_DATA(adapter)->bFWReady)
		rtw_write8(adapter, 0x4003, rtw_read8(adapter, 0x4003) & 0xdf);
	rtw_write8(adapter, 0x4080, 0);
	on = _FALSE;
	rtw_hal_set_hwreg(adapter, HW_VAR_APFM_ON_MAC, &on);
	RUN_POWER(adapter, s9082_Tag_close_run);
	rtw_write8(adapter, 0x401c, 0x70);
	GET_HAL_DATA(adapter)->bFWReady = _FALSE;
	rtw_hal_set_hwreg(adapter, HW_VAR_APFM_ON_MAC, &on);
}

static int firmware_read8(void *context, unsigned int address, unsigned char *value)
{
	return power_read(context, address, 0, value);
}

static int firmware_write8(void *context, unsigned int address, unsigned char value)
{
	return power_write(context, address, 0, value);
}

static int firmware_read32(void *context, unsigned int address, unsigned int *value)
{
	PADAPTER adapter = context;
	__le32 raw = 0;
	int result;
	if (address < 0x4100)
		result = sd_cmd52_read(&adapter->iopriv.intf, s9082_cmd_addr(address, NULL, NULL), 4, (u8 *)&raw);
	else {
		/* CMD53 scatterlists require a directly mapped, DMA-safe buffer. */
		u8 *buffer = rtw_zmalloc(4);
		if (!buffer) return 0;
		result = sd_read(&adapter->iopriv.intf, s9082_cmd_addr(address, NULL, NULL), 4, buffer);
		_rtw_memcpy(&raw, buffer, 4);
		rtw_mfree(buffer, 4);
	}
	*value = le32_to_cpu(raw);
	return result == 0;
}

static int firmware_write32(void *context, unsigned int address, unsigned int value)
{
	PADAPTER adapter = context;
	return rtw_write32(adapter, address, value) == _SUCCESS;
}

static int firmware_reset(void *context)
{
	u8 value, cpu;
#define RESET_READ(address, output) if (!firmware_read8(context, address, &output)) return 0
#define RESET_WRITE(address, value) if (!firmware_write8(context, address, value)) return 0
	RESET_READ(0x401c, value);
	RESET_WRITE(0x401c, value & 0xbf);
	RESET_READ(0x401d, value);
	RESET_WRITE(0x401d, value & 0xef);
	RESET_READ(0x4003, cpu);
	RESET_WRITE(0x4003, cpu & 0xdf);
	RESET_READ(0x401c, value);
	RESET_WRITE(0x401c, value & 0xbf);
	RESET_READ(0x401d, value);
	RESET_WRITE(0x401d, value | 0x10);
	RESET_WRITE(0x4003, cpu | 0x20);
#undef RESET_READ
#undef RESET_WRITE
	return 1;
}

static unsigned int firmware_time(void *context)
{
	return jiffies_to_msecs(jiffies);
}

static int firmware_stopped(void *context)
{
	PADAPTER adapter = context;
	return RTW_CANNOT_IO(adapter);
}

static void firmware_wait(void *context)
{
	rtw_msleep_os(1);
}

s32 s9082_firmware_download(PADAPTER adapter)
{
	struct s9082_fw_io io = {adapter, firmware_read8, firmware_write8,
		firmware_read32, firmware_write32, firmware_reset, firmware_time,
		firmware_stopped, firmware_wait};
	int result = s9082_fw_download(&io, s9082_nic_firmware, sizeof(s9082_nic_firmware));
	if (!result)
		RTW_ERR("S9082 stock firmware download/checksum/ready failed\n");
	return result ? _SUCCESS : _FAIL;
}

u32 s9082_read_board(PADAPTER adapter)
{
	struct s9082_fw_io io = {adapter, firmware_read8, firmware_write8,
		firmware_read32, firmware_write32, firmware_reset, firmware_time,
		firmware_stopped, firmware_wait};
	u8 physical[256];
	u8 *map = GET_HAL_DATA(adapter)->efuse_eeprom_data;
	unsigned int i;
	int ok = 0;
	if (s9082_power_on(adapter) != _SUCCESS) goto finish;
	if (rtw_write8(adapter, 0x40cf, 0x96) != _SUCCESS ||
	    rtw_write16(adapter, 0x4002, rtw_read16(adapter, 0x4002) | 0x800) != _SUCCESS ||
	    rtw_write16(adapter, 0x4008, rtw_read16(adapter, 0x4008) | 0x44) != _SUCCESS)
		goto finish;
	for (i = 0; i < ARRAY_SIZE(physical); i++)
		if (!s9082_efuse_byte(&io, i, physical + i)) goto finish;
	ok = s9082_efuse_decode(physical, sizeof(physical), map);
	if (ok && (map[0] != 0x82 || map[1] != 0x90)) ok = 0;
finish:
	rtw_write8(adapter, 0x40cf, 0);
	s9082_power_off(adapter);
	RTW_INFO("S9082 board efuse read result=%d\n", ok);
	return ok ? _SUCCESS : _FAIL;
}

static int run_init(PADAPTER adapter, const struct s9082_init_op *ops, unsigned int count)
{
	unsigned int i, raw;
	for (i = 0; i < count; i++) {
		const struct s9082_init_op *op = &ops[i];
		int result = _SUCCESS;
		if (RTW_CANNOT_IO(adapter))
			return _FAIL;
		switch (op->kind) {
		case 1: result = rtw_write8(adapter, op->address, op->value); break;
		case 2: result = rtw_write16(adapter, op->address, op->value); break;
		case 4: result = rtw_write32(adapter, op->address, op->value); break;
		case 5:
		case 6:
			raw = 0;
			if (op->mask != 0xffffffffU && !firmware_read32(adapter, op->address, &raw))
				return _FAIL;
			result = rtw_write32(adapter, op->address,
				s9082_bb_merge(raw, op->mask, op->value, op->kind == 5));
			break;
		case 7: power_delay(adapter, op->value); break;
		case 8: power_delay(adapter, op->value * 1000); break;
		case 9: rtw_msleep_os(op->value); break;
		default: return _FAIL;
		}
		if (result != _SUCCESS) {
			RTW_ERR("S9082 init failed at operation %u register %#x\n", i, op->address);
			return _FAIL;
		}
	}
	return _SUCCESS;
}

u32 s9082_mac_init(PADAPTER adapter)
{
	if (run_init(adapter, s9082_init_mac, ARRAY_SIZE(s9082_init_mac)) != _SUCCESS)
		return _FAIL;
	return rtw_write16(adapter, 0x44ca, 0xe0e0);
}

u32 s9082_phy_init(PADAPTER adapter)
{
	u16 value;
	value = rtw_read16(adapter, 0x4002);
	if (rtw_write16(adapter, 0x4002, value | 0x4c0) != _SUCCESS ||
	    rtw_write8(adapter, 0x401f, 0xe0) != _SUCCESS ||
	    rtw_write8(adapter, 0x4002, 0xc7) != _SUCCESS)
		return _FAIL;
	if (run_init(adapter, s9082_init_phy, ARRAY_SIZE(s9082_init_phy)) != _SUCCESS)
		return _FAIL;
	return run_init(adapter, s9082_init_agc, ARRAY_SIZE(s9082_init_agc));
}

u32 s9082_rf_init(PADAPTER adapter)
{
	u32 saved, raw;
	int result;
	if (!firmware_read32(adapter, 0x4870, &raw))
		return _FAIL;
	saved = (s9082_reverse_byte_bits(raw) >> 4) & 1;
	if (!firmware_read32(adapter, 0x4860, &raw) ||
	    !firmware_write32(adapter, 0x4860, s9082_bb_merge(raw, 0x100000, 1, 1)))
		return _FAIL;
	power_delay(adapter, 1);
	if (!firmware_read32(adapter, 0x4860, &raw) ||
	    !firmware_write32(adapter, 0x4860, s9082_bb_merge(raw, 0x10, 1, 1)))
		return _FAIL;
	power_delay(adapter, 1);
	if (!firmware_read32(adapter, 0x4824, &raw) ||
	    !firmware_write32(adapter, 0x4824, s9082_bb_merge(raw, 0x400, 0, 0)))
		return _FAIL;
	power_delay(adapter, 1);
	if (!firmware_read32(adapter, 0x4824, &raw) ||
	    !firmware_write32(adapter, 0x4824, s9082_bb_merge(raw, 0x800, 0, 0)))
		return _FAIL;
	power_delay(adapter, 1);
	result = run_init(adapter, s9082_init_rf, ARRAY_SIZE(s9082_init_rf));
	if (!firmware_read32(adapter, 0x4870, &raw) ||
	    !firmware_write32(adapter, 0x4870, s9082_bb_merge(raw, 0x10, saved, 1)))
		return _FAIL;
	/* TX-power tracking callback setup remains to integrate separately. */
	return result;
}

u32 s9082_rf_read(PADAPTER adapter, u32 reg)
{
	u32 saved = rtw_read32(adapter, 0x4824), select;
	rtw_write32(adapter, 0x4824, saved & 0x7fffffff);
	rtw_udelay_os(10);
	rtw_write32(adapter, 0x4824, ((reg & 255) << 23) | 0x80000000 | (saved & 0x807fffff));
	rtw_udelay_os(100);
	rtw_udelay_os(10);
	select = s9082_reverse_byte_bits(rtw_read32(adapter, 0x4820));
	return s9082_reverse_byte_bits(rtw_read32(adapter, (select & 0x100) ? 0x48b8 : 0x48a0)) & 0xfffff;
}

void s9082_rf_write(PADAPTER adapter, u32 reg, u32 mask, u32 value)
{
	unsigned int shift = 0;
	if (!mask) return;
	while (!(mask & (1U << shift))) shift++;
	if (mask != 0xfffff)
		value = (s9082_rf_read(adapter, reg) & ~mask) | (value << shift);
	rtw_write32(adapter, 0x4840, s9082_reverse_byte_bits((value & 0xfffff) | ((reg & 255) << 20)));
}

static int s9082_llt_write(PADAPTER adapter, unsigned int address, unsigned int next)
{
	unsigned int retry;
	if (rtw_write32(adapter, 0x41e0,
	    s9082_reverse_byte_bits(0x40000000U | (address << 8) | next)) != _SUCCESS) return 0;
	for (retry = 0; retry < 20; retry++)
		if (!(rtw_read32(adapter, 0x41e0) & 0x03000000)) return 1;
	return 0;
}

u32 s9082_receive_init(PADAPTER adapter)
{
	PHAL_DATA_TYPE hal = GET_HAL_DATA(adapter);
	unsigned int i;
	RTW_INFO("S9082 START power\n");
	if (s9082_power_on(adapter) != _SUCCESS) goto fail;
	RTW_INFO("S9082 START firmware\n");
	if (s9082_firmware_download(adapter) != _SUCCESS)
		goto fail;
	hal->bFWReady = _TRUE;
	hal->fw_ractrl = _FALSE;
	RTW_INFO("S9082 START MAC\n");
	if (s9082_mac_init(adapter) != _SUCCESS) goto fail;
	RTW_INFO("S9082 START PHY\n");
	if (s9082_phy_init(adapter) != _SUCCESS) goto fail;
	RTW_INFO("S9082 START RF\n");
	if (s9082_rf_init(adapter) != _SUCCESS) goto fail;
	RTW_INFO("S9082 START queues\n");
#define W8(a, v) do { if (rtw_write8(adapter, a, v) != _SUCCESS) goto fail; } while (0)
#define W16(a, v) do { if (rtw_write16(adapter, a, v) != _SUCCESS) goto fail; } while (0)
#define W32(a, v) do { if (rtw_write32(adapter, a, v) != _SUCCESS) goto fail; } while (0)
	W8(0x4214, 0);
	W32(0x4200, s9082_reverse_byte_bits(0x80ee0900));
	RTW_INFO("S9082 queue allocation raw=%#x\n", rtw_read32(adapter, 0x4200));
	W16(0x4116, 0xbcff); W8(0x4104, 0x88);
	SdioLocalCmd52Write1Byte(adapter, 0x0000, 4);
	SdioLocalCmd52Write1Byte(adapter, 0x0080, 0);
	W8(0x4424, 0x1f); W8(0x4425, 0x1f); W8(0x445d, 0x1f); W8(0x4114, 0x1f);
	W8(0x4209, 0xf8);
	for (i = 0; i < 0xf7; i++) if (!s9082_llt_write(adapter, i, i + 1)) goto fail;
	if (!s9082_llt_write(adapter, 0xf7, 255) || !s9082_llt_write(adapter, 255, 0xf8)) goto fail;
	for (i = 0xf8; i < 255; i++) if (!s9082_llt_write(adapter, i, i + 1)) goto fail;
	for (i = 0; i < 256; i++) {
		u32 value = 0;
		unsigned int retry, expected = i == 0xf7 ? 255 : (i == 255 ? 0xf8 : i + 1);
		W32(0x41e0, s9082_reverse_byte_bits(0x80000000U | (i << 8)));
		for (retry = 0; retry < 20; retry++) {
			value = s9082_reverse_byte_bits(rtw_read32(adapter, 0x41e0));
			if (!(value & 0xc0000000)) break;
		}
		if (retry == 20 || (value & 255) != expected) {
			RTW_INFO("S9082 LLT mismatch index=%u expected=%u value=%#x\n", i, expected, value);
			goto fail;
		}
	}
	RTW_INFO("S9082 LLT all 256 entries verified\n");
	/* Stock _hal_init_tll9083xs enables TX reports before receive setup. */
	W8(0x44ec, rtw_read8(adapter, 0x44ec) | 0xc0);
	W8(0x44ed, 0x40);
	W16(0x44f0, 0xbc0f);
	/* Stock three-pipe priority: BE=1, BK=1, VI=2, VO=3, MG=3, HI=3. */
	W16(0x410c, (rtw_read16(adapter, 0x410c) & 0xe0) |
		s9082_reverse_byte_bits((1U << 8) | (1U << 10) | (2U << 6) |
			(3U << 4) | (3U << 12) | (3U << 14)));
	W8(0x460f, 0x20);
	hal->ReceiveConfig = 0x7000600f;
	W32(0x4608, s9082_reverse_byte_bits(hal->ReceiveConfig));
	W16(0x46a4, 0xffff); W16(0x46a2, 0x2000); W16(0x46a0, 0xffff);
	W8(0x4281, 0x60); W8(0x4280, 0x24);
	W8(0x410c, rtw_read8(adapter, 0x410c) | 0x20);
	W32(0x4800, s9082_bb_merge(rtw_read32(adapter, 0x4800), 0x03000000, 3, 1));
	W8(0x4100, rtw_read8(adapter, 0x4100) | 3);
	hal->RfRegChnlVal[0] = 0x7c01;
	s9082_rf_write(adapter, 0x18, 0xfffff, hal->RfRegChnlVal[0]);
	hal->current_channel = 1;
	hal->current_channel_bw = CHANNEL_WIDTH_20;
	adapter_to_pwrctl(adapter)->rf_pwrstate = rf_on;
	InitInterrupt8188ESdio(adapter);
	HalQueryTxBufferStatus8189ESdio(adapter);
	rtw_hal_set_sdio_tx_max_length(adapter,
		hal->SdioTxFIFOFreePage[0], hal->SdioTxFIFOFreePage[1],
		hal->SdioTxFIFOFreePage[2], hal->SdioTxFIFOFreePage[3], SDIO_TX_DIV_NUM);
	HalQueryTxOQTBufferStatus8189ESdio(adapter);
	hal->SdioTxOQTMaxFreeSpace = hal->SdioTxOQTFreeSpace;
	RTW_INFO("S9082 receive initialization completed\n");
	return _SUCCESS;
fail:
	s9082_power_off(adapter);
	return _FAIL;
#undef W8
#undef W16
#undef W32
}
