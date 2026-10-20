/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __RTW_S9082_HAL_H__
#define __RTW_S9082_HAL_H__
u32 s9082_power_on(PADAPTER adapter);
void s9082_power_off(PADAPTER adapter);
s32 s9082_firmware_download(PADAPTER adapter);
u32 s9082_mac_init(PADAPTER adapter);
u32 s9082_phy_init(PADAPTER adapter);
u32 s9082_rf_init(PADAPTER adapter);
u32 s9082_read_board(PADAPTER adapter);
u32 s9082_rf_read(PADAPTER adapter, u32 reg);
void s9082_rf_write(PADAPTER adapter, u32 reg, u32 mask, u32 value);
u32 s9082_receive_init(PADAPTER adapter);
#endif
