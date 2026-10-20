# Standalone staged diagnostic

Historical bring-up harness, separate from the working network driver. It binds
the same SDIO device, executes the selected stage, attempts shutdown and releases
the binding. It does not register a network interface. Build with the supplied
Makefile and matching prepared target headers after extracting firmware.

Stages: 0 register reads; 1 power; 2 firmware; 3 mailbox; 4 MAC/PHY/AGC;
5 RF table; 6 read-only efuse; 7 RF channel roundtrip; 8 passive receive polling.
Stages 7 and 8 include RF initialization; stage 6 bypasses firmware initialization.
The efuse stage prints device-specific bytes into the kernel log.

The driver must release the device before this diagnostic can bind. Its receive
setup is a historical experiment, not equivalent to the current network driver
initialization or a replacement for the documented connectivity checks.
