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

Initial bulk-upload traces showed MCS7 with `ampdu=0`, but the trace budget
was consumed at the beginning of the transfer. The follow-up below corrects
that interpretation. CCX ACK reports remain unresolved; failfast stays enabled.
High loaded download latency and signal-dependent rate behavior remain open.

## Follow-up: SCI AMPDU limits — 2026-10-21

After an eight-second untraced upload warm-up, station state showed TID0
aggregation enabled, no candidate TID pending, and RX reorder size16.
Tracing the next transfer observed **MCS7, SGI1, AMPDU1**. ADDBA negotiation
and descriptor aggregation enablement therefore work on this setup; the
earlier samples did not establish a negotiation failure. Descriptor state
alone does not prove the exact over-air aggregate size.

The runtime AMPDU-factor control nevertheless fell through to generic
Realtek registers. SCI now implements stock `hw_reg_9083x_set` case0x22
(same evidence ID above): start with raw bytes `41,15,4e,9d`, cap each
nibble to `min(1 << (factor + 2), 15)`, and write0x4458–0x445b without
bit reversal. Factors outside0–3 are rejected. Negotiated factor1 produced
`41,15,48,88`, confirmed by the successful write-path trace.

With this isolated override, three30-second single-stream tests measured
**44.50–46.44Mbps upload**, **30.86–30.97Mbps download**; four streams
measured42.35/31.00Mbps. All loaded Wi-Fi gateway pings replied; mean
upload-loaded latency was26–34ms, download-loaded225–318ms.

A separate alternating baseline/patched/baseline/patched comparison used
fresh module loads and20-second uploads:

| Run | Previous report-control driver | SCI AMPDU-factor override |
| --- | --- | --- |
| Pair1 | 33.11Mbps | 45.35Mbps |
| Pair2 | 30.36Mbps | 42.23Mbps |

TCP retransmissions occurred in both variants (24/118 baseline,29/68
patched); no claim of eliminating retransmissions is made. The repeated
upload improvement supports retaining the SCI register correction.

Validated module SHA256:
`a3544a007ceb3c8205cbd29e234df9df23bbaa5ca99d83615a705446ce6d8c7a`.
Cross-build and host contracts passed. Regression:20/20 active/passive
scans (1.54–1.56s),300/300 large pings,HTTPS200,reconnect+scan,10/10
subsequent pings, empty queues and16 free TX buffers. Cold boot untested.

## Download-path experiments — 2026-10-21

An experimental SCI EDCA override implemented stock cases0x1c–0x1f at
0x4500/04/08/0c with per-byte reversal. Runtime traces confirmed the
negotiated values were written. Three download runs measured28.31–29.08Mbps,
versus30.86–30.97Mbps in the earlier AMPDU baseline; upload remained
44.73–46.41Mbps. This did not establish a download benefit. The experimental
override was removed and the installed AMPDU baseline reloaded.

Using that experimental build, alternating RX reorder windows16/64/16/64
measured31.04/30.77/30.72/30.72Mbps download. No benefit was demonstrated;
automatic RX acceptance/window settings were restored. No RX DMA register
toggle was attempted in this follow-up.

On the restored AMPDU baseline, explicit iperf3 socket buffers greatly
reduced download-loaded gateway latency without increasing throughput:

| iperf3 `-w` | Download Mbps | Loaded gateway mean / maximum ms |
| --- | --- | --- |
| 32K | 26.69 | 9.50 / 30.65 |
| 64K | 28.94 | 19.82 / 31.08 |
| 128K | 29.36 | 35.35 / 55.66 |
| 256K | 28.99 | 42.21 / 77.33 |

Each was a20-second device-bound reverse TCP run with concurrent gateway
pings. Buffer settings applied only to the test sockets; no system TCP
defaults were changed. These observations support queue buildup as a major
contributor to loaded latency, but do not localize that queue to the driver,
access point, or sender. They do not explain the roughly31Mbps download
ceiling. The Windows sender, AP scheduling, receive delivery costs and
over-air RX rates still need isolation. No download-speed fix is claimed.
After baseline restoration, automatic RX settings and5/5 Wi-Fi internet
pings were verified. The installed driver remains the validated AMPDU build.

## Stock receive filtering — 2026-10-21

Stock `WMACSetting_init` uses logical RCR0x700060ce, raw0x0e000673:
`ev_0c0c4a6367b2828461b20730abfb3b9cd43a3c881c5a873a87d614deea7da63f`.
The port instead retained0x7000600f: accept-all-unicast with both BSSID
checks disabled. Restore the stock station mask, clear the BSSID checks
during surveys (0x7000600e), and restore them at survey completion.
This prevents unnecessary unrelated-unicast reception without suppressing
other BSS discovery during scans. After scan/reconnect regression, register
0x4608 read back0x0e000673, matching stock station filtering.

Three30-second device-bound LAN runs measured31.39–31.67Mbps download
and43.45–45.75Mbps upload; four streams measured31.42/39.66Mbps
download/upload. This is only a small download difference from earlier
30.86–30.97Mbps results, not proof the remaining bottleneck is fixed.
Loaded single-stream download gateway mean latency remained211–289ms.

Before the patch, a20-second29.57Mbps download showed50263 received data
frames at descriptor rate16 (MCS4),2268 at17 and15 at18. RX rates are
descriptor observations, not inferred from the displayed TX rate. CPU0
retained roughly36% idle time over that sample; MMC host interrupt count
increased138118. Interface RX error/drop counters remained zero. These
do not establish a purely CPU-bound receive bottleneck. The stock RX
descriptor parser matches the port's key rate, length and shift fields.

Validated experimental module SHA256 (before a comment-only source edit):
`c982b537c49ba2c7dc8c810bf4a1bbe00575c9e34b3aceb3dad6d6049be99e5a`.
Cross-build/host contracts passed;20/20 scans,300/300 large pings,HTTPS200,
reconnect+scan,10/10 pings, empty queues and16 free buffers passed. Cold
boot untested. The download ceiling and actual receive aggregate sizes
remain unresolved; no RX DMA aggregation toggle was retained.

## RX FIFO and timing experiments — 2026-10-21

Temporary counters measured FIFO reads, bytes and size buckets during
20-second device-bound downloads. They measure SDIO batches, not over-air
AMPDU sizes. The instrumented station-filter baseline measured28.36Mbps,
38936 reads and83944392 FIFO bytes (2156 bytes/read);27361 reads were
at most2KB,11571 were2–8KB and4 exceeded8KB.

Isolated stock `EDCA_init` SIFS writes (raw0x0850 to0x4428,0x463a,
0x4514,0x4516) measured28.57Mbps and2205 bytes/read. No meaningful
improvement was demonstrated. Stock evidence:
`ev_7a31b4358fe09e85f5fba5862a5384f8817f9940124cb2c2d008df034cca929f`.

The alternate stock `SdioAggregationSetting_init` profile uses raw
0xf0/0x80 at0x4281/0x4280 rather than normal0x60/0x24. Applying it only
at module initialization measured28.99Mbps and1747 bytes/read, with48730
reads. This alternate profile belongs to a different stock configuration;
it did not demonstrate a useful download improvement here. Evidence:
`ev_f9e76ce7898752c925da548535779f79c1ba56d7d47c8d0a8501627917825d16`.

Both experiments and temporary counters were removed. The installed
station-filter baseline was reloaded and5/5 internet pings passed.
No new performance fix is claimed. Further work needs actual over-air
RX-rate/retry/aggregate evidence and a focused PHY/calibration comparison.
The72.2Mbps displayed PHY rate is not an achievable TCP payload target:
protocol overhead, acknowledgments, contention and retries consume airtime.
