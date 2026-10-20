#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
headers=${KSRC:-/lib/modules/$(uname -r)/build}
test -f "$root/hal/rtl8188e/sdio/s9082_firmware_generated.h" || {
    echo 'First run: python3 tools/extract-firmware.py /path/to/stock-9082xs.ko' >&2; exit 1;
}
set -- -C "$root" KSRC="$headers" CONFIG_S9082=y CONFIG_PLATFORM_I386_PC=n \
    CONFIG_PLATFORM_RPI_ARM64=y CONFIG_AP_MODE=n CONFIG_P2P=n \
    CONFIG_MP_INCLUDED=n CONFIG_POWER_SAVING=n CONFIG_WIFI_MONITOR=n
if test -n "${ARCH:-}"; then set -- "$@" ARCH="$ARCH"; fi
if test -n "${CROSS_COMPILE:-}"; then set -- "$@" CROSS_COMPILE="$CROSS_COMPILE"; fi
make "$@" -j"${JOBS:-2}"
sha256sum "$root/8189es.ko"
modinfo -F vermagic "$root/8189es.ko"
