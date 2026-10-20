#!/bin/sh
set -eu
kernel=${KVER:-$(uname -r)}
headers=${KSRC:-/lib/modules/$kernel/build}
printf 'Kernel: %s\nArchitecture: %s\nHeaders: %s\n' "$kernel" "$(uname -m)" "$headers"
for command in make python3 "${CROSS_COMPILE:-}gcc" modinfo sha256sum; do
    command -v "$command" >/dev/null || { echo "Missing prerequisite: $command" >&2; exit 1; }
done
test -f "$headers/Makefile" || { echo 'Install matching kernel development headers or set KSRC.' >&2; exit 1; }
found=0
for device in /sys/bus/sdio/devices/*; do
    test -f "$device/vendor" || continue
    if test "$(cat "$device/vendor")" = 0x02e7 && test "$(cat "$device/device")" = 0x9082; then
        printf 'Supported SDIO device: %s\n' "$device"
        test ! -e "$device/driver" || readlink "$device/driver"
        found=1
    fi
done
test "$found" = 1 || echo 'No 02e7:9082 device detected on this build host (cross-builds are allowed).'
