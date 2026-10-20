#!/usr/bin/env python3
"""Extract the supported firmware from a user-supplied stock module."""
import argparse
import hashlib
from pathlib import Path
import sys
from elftools.elf.elffile import ELFFile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('stock_module', type=Path, help='Supported user-supplied 9082xs.ko')
source = parser.parse_args().stock_module
expected = 'b285a592d7cacad018ca4bd3451da967f225f2db5eb109c55957289e38d050cc'
if hashlib.sha256(source.read_bytes()).hexdigest() != expected:
    sys.exit('Unsupported stock module SHA256; refusing unknown layout')
with source.open('rb') as stream:
    section = ELFFile(stream).get_section_by_name('.data')
    firmware = section.data()[0x49b0:0x49b0 + 15382]
if hashlib.sha256(firmware).hexdigest() != 'f26c7e38bd498dee77fb302983a7df7d743e68ffc867624a18237eefc21d409c':
    sys.exit('Firmware digest mismatch')
target = Path(__file__).resolve().parents[1] / 'hal/rtl8188e/sdio/s9082_firmware_generated.h'
lines = ['/* Locally extracted; excluded from Git. */', 'static const unsigned char s9082_nic_firmware[] = {']
for offset in range(0, len(firmware), 16):
    lines.append('\t' + ', '.join('0x%02x' % b for b in firmware[offset:offset + 16]) + ',')
lines.append('};')
target.write_text('\n'.join(lines) + '\n')
print('Verified firmware extracted to', target)
