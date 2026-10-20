#!/bin/sh
set -eu
case "${1:-}" in ''|--autoload) ;; *) echo 'Usage: install.sh [--autoload]' >&2; exit 2 ;; esac
test "$#" -le 1 || { echo 'Usage: install.sh [--autoload]' >&2; exit 2; }
test "$(id -u)" = 0 || { echo 'Run with sudo.' >&2; exit 1; }
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
kernel=$(uname -r)
test -f "$root/8189es.ko"
test "$(modinfo -F vermagic "$root/8189es.ko" | cut -d ' ' -f1)" = "$kernel" || { echo 'Kernel/module mismatch' >&2; exit 1; }
target=/lib/modules/$kernel/updates/s9082/8189es.ko
test ! -e "$target" || cp -p "$target" "$target.backup-$(date +%s)"
install -D -m 0644 "$root/8189es.ko" "$target"
depmod -a "$kernel"
if test "${1:-}" = --autoload; then
    test ! -e /etc/modules-load.d/s9082.conf || { echo 'Existing autoload file preserved'; exit 0; }
    printf '8189es\n' > /etc/modules-load.d/s9082.conf
fi
echo 'Installed. Load with sudo modprobe 8189es, then configure your network.'
