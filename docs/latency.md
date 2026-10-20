# S905X latency investigation

## Paired idle measurements

100 packets per interface/destination, Ethernet and Wi-Fi measured concurrently
for the same destination, without scans. Small default ICMP payloads:

| Path | Ethernet median | Wi-Fi median | Wi-Fi p95 |
| --- | ---: | ---: | ---: |
| Gateway, original configuration | 0.818 ms | 10.7 ms | 21.0 ms |
| 1.1.1.1, original configuration | 28.4 ms | 39.7 ms | 48.3 ms |
| Gateway, native SDIO IRQ enabled | 0.758 ms | 5.925 ms | 9.18 ms |
| 1.1.1.1, native SDIO IRQ enabled | 28.4 ms | 33.3 ms | 37.3 ms |

All four paths in each comparison received 100/100 replies. The IRQ-enabled
internet run still had a 222 ms outlier. This improves latency, but does not
establish consistently sub-5 ms LAN or 20–30 ms internet latency.

## Missing board capability

The running S905X DT node `/soc/apb@d0000000/mmc@70000` lacked the boolean
`cap-sdio-irq`. Its mmc0 capabilities were `0x400f0105`, without
`MMC_CAP_SDIO_IRQ` (bit 3), and `ksdioirqd/mmc0` was polling interrupts.
Linux v6.18 `drivers/mmc/core/sdio_irq.c` uses an adaptive polling loop with
a 10 ms idle interval when this capability is absent.

A temporary live-tree property plus reprobe of only the Wi-Fi controller
enabled native interrupts: caps `0x400f010d`, caps2 `0x00060000`, no polling
thread. This removed roughly 5–6 ms from typical RTT. SDIO remained 4-bit,
50 MHz. This is a board configuration correction, not a signal-strength fix
or a portable driver-side capability override.

Afterward, 10 active and 10 passive scans, reconnect, 300/300 large pings,
interface-bound HTTPS200 and TX-queue drainage passed. Large 1000-byte ping
RTT averaged 42.7 ms; that is a different payload from the small-ping table.

## Remaining delay

Reported Wi-Fi power saving was off. Temporarily changing CPU governor from
`schedutil` to `performance` reduced a separate 60-packet local mean from
6.040 ms to 4.885 ms; restoring `schedutil` gave 5.933 ms. The governor was
restored, not made permanent. CPU policy accounts for some additional delay,
but the rest of the wireless/firmware/SDIO pipeline remains to be measured.

A live receive-aggregation toggle gave little benefit and disrupted reception
when restored; reloading the validated driver recovered connectivity. No
aggregation change was retained. Do not treat that toggle as a latency fix.

## Persistent board change and verification boundary

On the test box, a separate copy of the selected DTB adds only
`cap-sdio-irq` to the Wi-Fi controller. The original DTB is preserved, and
the SD-card `/boot/uEnv.txt` selection has a timestamped backup. The copied
DTB is selected for the next boot. **Cold boot of this selection is not yet
verified**; runtime IRQ operation was validated through controller reprobe.

For other boards, identify the correct controller and check host-driver IRQ
support before preparing a board-specific DT fix. The driver installer does
not edit device trees. Retain Ethernet/console access and the original boot
selection. After cold boot, check the host capability bit, connectivity,
scan behavior and paired latency again. Do not unload a temporary live-DT
property provider while a controller instantiated from that property is active.
