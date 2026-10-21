# SCI station-port completion checklist

No percentage-complete claim is used. Connection and a passing test set do
not establish full hardware compatibility. Scope is the tested station-only
build on S905X/kernel6.18.44-ophub; other modes need independent validation.

## Verified corrections

- SDIO address/access encoding, firmware load, stock initialization records.
- LLT/queue allocation, station MAC/BSSID, TX buffer limits.
- Software rate-state initialization, report controls and AMPDU limits.
- Station receive filtering and scan filter restoration.
- Board native SDIO IRQ and cold-start networkd/WPA connection.
- Watchdog DMA/status addresses corrected to stock SCI contracts (below).

## Open work and acceptance evidence

| Area | Current gap | Evidence needed before closure |
| --- | --- | --- |
| RF access | Extended/mode-table reads not established | Verified access and restoration for RF0xef/30/31/32, not zero dump assumptions |
| IQK | Private prototype regressed; not in public build | Verified prerequisites/cleanup; staged no-matrix/TX/RX trials; stable repeated initialization |
| LC/power tracking/TX power | Not fully integrated | Stock lifecycle, efuse inputs, calibration/power behavior validated |
| Management ACK reports | No real CCX completion observed; failfast workaround | Actual report generation, parsing and successful/failed ACK semantics |
| Scanning | One20-second cold-start scan timeout, repeat passed | Reproduce with request/event traces and close completion race/failure |
| Security/CAM | Generic register paths incompletely adapted | Key install/update/remove, reconnect/rekey and offload/software behavior verified |
| MLME/rate/timing controls | Remaining generic fallthroughs | Reachability inventory and stock-backed address/encoding checks for each station operation |
| Reset/shutdown | Watchdog addresses fixed; recovery/reboot not validated | Recovery-path audit and warm-reboot verification with physical recovery available |
| Download performance | About26–32Mbps; lower RX rates observed | RX rate/retry/aggregate evidence and controlled corrections |
| Stability | Short regressions pass; long soak incomplete | Sustained bidirectional load, reconnect/rekey and repeated boot tests |

## Watchdog log-flood correction — 2026-10-21

The inherited link watchdog read Realtek0x0288, logged0xeaeaeaea every
roughly two seconds, then wrote the invalid value back. It also read
Realtek firmware byte0x01c8. The inherited TX check read0x0210.

Stock SCI `_sreset_linked_status_check_tll9083x` reads/writes **raw0x4288**
and reads **0x41c8** without decoding Realtek firmware error meanings.
`_check_sreset_xmit_status_tll9083x` reads **raw0x4210**. Evidence:
`ev_bf250da717a5ddbf68120f1a44aef33d0f8ca14c5a1a57e8fb8d8b686ddfd334`,
`ev_52d360ff3058243400a65994470b562ffa8e2fd57ad0f2686acd9f7f4a81c269`.

SCI guards now select those addresses. RX nonzero status retains stock
read/writeback, with rate-limited diagnostics and no writeback for invalid
0xeaeaeaea/0xffffffff values. Existing debug reset triggers remain present;
they were not injected because the reset implementation needs its own audit.
This is not fault-injection validation of reset recovery.

Runtime0x4210/0x4288/0x41c8 each read zero. Tested module SHA256:
`3264488b248897c7e090d485382930964f8ba069792d0bfb83787494ceee0323`.
Cross-build and host contracts passed;20/20 scans,300/300 large pings,
HTTPS200,reconnect+scan,10/10 pings, empty queues and16 free buffers.
After these tests, the previous link-watchdog message had zero occurrences
in the last two minutes. Existing historical dmesg records are not erased.
Cold boot of this artifact is untested. `issue_nulldata` attempt messages
are separate from invalid DMA-status logging and remain unchanged.
