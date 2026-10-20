#!/bin/sh
set -eu
test "$(id -u)" = 0
kernel=$(uname -r)
if test -d /sys/module/8189es; then modprobe -r 8189es; fi
rm -f "/lib/modules/$kernel/updates/s9082/8189es.ko"
if test -f /etc/modules-load.d/s9082.conf && test "$(cat /etc/modules-load.d/s9082.conf)" = 8189es; then
    rm /etc/modules-load.d/s9082.conf
fi
depmod -a "$kernel"
echo 'Driver removed for current kernel. Network profiles and backups retained.'
