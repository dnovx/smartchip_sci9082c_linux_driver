# Local throughput and SCI rate feedback — 2026-10-20

## Test method

S905X kernel 6.18.44-ophub, native SDIO IRQ enabled, strong Wi-Fi signal.
The wired peer ran the community ar51an Windows iperf3 3.22 build.
Archive SHA256:
`c9feabb3d721039508ccb81f74c2ada3ae11b9e75d33d24510573c2aa21da420`.

Linux iperf3 must use **both `-B ADDRESS` and `--bind-dev INTERFACE`**.
Source-address binding alone selected Ethernet for Wi-Fi-source uploads in
this dual-connected setup; those preliminary results are excluded.
Each single-stream direction used three 30-second TCP runs, plus one
four-stream run. Concurrent interface-bound gateway pings sampled loaded
latency. Windows did not answer peer ICMP, so latency is to the gateway,
not the throughput server. Raw network-identifying logs remain private.

## Report-control correction

Generic runtime controls still used Realtek register addresses even though
SCI initialization programmed its own registers. SCI-only overrides now set:

- `HW_VAR_RPT_TIMER_SETTING`: 16-bit write to **0x44f0**, reversing bits
  within each byte. Stock `TDB_TxRPT_TimeRA_Set_` evidence:
  `ev_0f1ac6049fcfcd8f6bc910898b8665af3bb1a2f3ce36c05573f393b3f7f758e5`.
- `HW_VAR_TX_RPT_MAX_MACID`: software-RA-only write of byte-reversed
  `max_macid + 1` to **0x44ed**. Stock `hw_reg_9083x_set`, case0x42:
  `ev_6bb77caf1f779380897adc43dd29302ef9e93ec24aed15988c427bf790f7b75d`.

Runtime traces verified interval37500 encoded/read back as0x493e and
max-MACID1 encoded/read back as0x40. Reports carried 16 bytes, validity
mask1, and plausible MACID0 successful/retry counts. SCI report handling
also rejects zero, partial, or oversized entry arrays before indexing RA
state. Optional metadata traces share the existing 600-record load budget.
This verifies programming and report delivery, not full rate adaptation
under changing signal conditions.

## Results

All figures are receiver Mbps. Baseline already includes the software-RA
initialization fix; this patch is a correctness fix, not a demonstrated
throughput improvement.

| Configuration | Single-stream upload | Single-stream download | Four-stream upload/download |
| --- | --- | --- | --- |
| Software-RA baseline | 33.77–34.18 | 29.22–29.32 | 30.84 / 29.18 |
| Final report-control patch | 32.96–33.76 | 28.90–29.11 | 32.47 / 31.07 |

An intermediate report-control build measured33.62–33.78 upload and
29.43–29.60 download. Final Ethernet controls measured94.30 upload and
94.89 download. Wi-Fi loaded gateway ping had zero loss in every final
run; single-stream mean latency was33–35ms during upload and391–394ms
during download. One upload run recorded104 TCP retransmissions, the other
two1 and0; this warrants longer observation rather than a zero-loss TCP claim.
Ethernet reverse-load pings suffered severe delay/loss despite94.9Mbps TCP;
this also occurred before the patch. Its cause is not established, so those
latency measurements cannot isolate a Wi-Fi-driver issue.

Final module SHA256:
`0ed9e55a655caee0e58c1e297ffdf46d8cfc45ece06eddc865e7d9e1e52e2799`.
Cross-build and host contracts passed. Runtime regression:20/20 scans,
300/300 large pings, HTTPS200, reconnect+scan,10/10 subsequent pings,
queues empty and16 TX buffers free. Cold boot of this artifact is untested.

## Next investigation

Bulk-upload descriptor traces showed MCS7 with `ampdu=0`, not just idle
ping traffic. Verify ADDBA negotiation, negotiated per-TID state and
SCI aggregation contracts before changing AMPDU limits. CCX ACK reports
remain unresolved; the failfast workaround remains enabled. High loaded
download latency and signal-dependent rate behavior remain open.
