#!/bin/sh
set -eu
interface=${1:-wlan0}
iw dev "$interface" link | grep -q '^Connected to ' || { echo 'Not associated' >&2; exit 1; }
ip -4 -o address show dev "$interface" | grep -q 'inet ' || { echo 'No IPv4 address' >&2; exit 1; }
ping -I "$interface" -c 10 -W 3 1.1.1.1
code=$(curl --interface "$interface" --connect-timeout 10 --max-time 30 -sS -o /dev/null -w '%{http_code}' https://example.com/)
test "$code" = 200 || { echo "HTTPS failed: $code" >&2; exit 1; }
echo 'PASS: association, IPv4, interface-bound ICMP and HTTPS'
