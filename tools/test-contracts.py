#!/usr/bin/env python3
"""Run hardware-independent transport and lifecycle contracts with host GCC."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='s9082-contract-') as work:
    executable = str(Path(work) / 'contract')
    subprocess.run(['gcc', '-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=undefined', '-I' + str(root / 'include'),
                    str(root / 'tools/test-contracts.c'), '-o', executable], check=True)
    subprocess.run([executable], check=True)
