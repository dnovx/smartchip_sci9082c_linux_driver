# Porting milestone — 2026-10-20

## What made the chipset work

1. Linux6.18 compatibility: timer/container recovery, cfg80211 signatures,
   regulatory guards, string conversion and crypto symbol collision fixes.
2. Explicit S9082 build selection and 02e7:9082 alias; distinct SDIO command
   device mapping and access-specific byte/nibble reversal.
3. Stock power records, firmware paging/control and mailbox protocol.
4. Low-register CMD52 access for efuse; allocated CMD53 destination fixes
   Meson MMC scatterlist/cache fault caused by stack-backed buffers.
5. Stock MAC/PHY/AGC/RF initialization, LLT and receive interrupt delivery.
6. Correct RCR get/set and mixed MAC/BSSID encoding for association.
7. Disable unsupported fifth management queue in the four-AC SDIO TX loop:
   fixes out-of-bounds queue access during WPA/EAPOL.
8. Free-page polling instead of unadapted availability interrupt setup;
   initialize software TX length limits from hardware page counts.
9. Correct LLT write operation and verify all256 links by readback.
10. Program stock hardware queue priority at0x410c. This was decisive for
    eliminating TX page exhaustion after approximately60 small packets.

Queue priority evidence:
`ev_92932b46d14a05a9ad78217d36fba1eda56fa1decaa335a325a610bfc3664250`.
BE/BK=1, VI=2, VO/MG/HI=3 with access-specific transformation.

## Runtime validation

Test platform: S905X AArch64, SDIO02e7:9082, kernel6.18.44-ophub.
Final installed working module SHA256:
`be2d61ac171223b98d4aeab491b8fb8032b270ba596754e17eae650f92928302`.
The public tree later splits the firmware into a locally generated header;
rebuilt hashes are not promised identical to that historical artifact.

Legacy mode:1000/1000 large pings; HT:300/300 then1500/1500 large pings over
five minutes; interface-bound HTTPS200; queues empty and16 TX buffers free.
Reconnect10/10 pings+HTTPS. Cold boot autoload/autoconnect300/300 large pings
andHTTPS. NetworkManager removed on test system; Netplan/networkd+generated
wpa_supplicant also passed DHCP and interface-bound ping/HTTPS.
The cold-boot test preceded migration to networkd; cold boot with the new
network backend still needs verification.

## Remaining adaptation

- Associated direct scan watchdog failures have a validated bounded-wait
  workaround below; actual CCX ACK-report delivery remains unresolved.
- Generic RTL8188E accesses still produce invalid0xeaea values: CAM/security,
  RXDMA reset checks, EDCA and management ACK handling need evidence-backed
  adaptation. Successful encrypted traffic does not prove hardware CAM works.
- Warm reboot did not return; cold power-on works. Cause unresolved.
- Calibration, power management, extended bandwidth/features, long-duration
  soak and TCP throughput require independent validation.

The historical “98%” was an engineering estimate, not a compatibility score.

## Automatic-rate initialization fix

SCI bring-up returns from `rtl8188es_hal_init()` through
`s9082_receive_init()`, bypassing the generic PHYDM initialization. Data TX
nevertheless obtains its descriptor rate from PHYDM's per-MACID software state.
Initialize that state with `odm_ra_info_init_all()` after SCI queue setup.
This sets initial rates, masks, thresholds and retry/power-training state; it
does not run unadapted generic PHYDM hardware initialization or force a rate.

Paired Ookla tests used the same server (70730), strong signal and the same
network configuration. Results are Mbps, download/upload:

| Configuration | Wi-Fi | Ethernet control |
| --- | --- | --- |
| Before fix, run 1 | 7.68 / 0.75 | 93.06 / 93.97 |
| Before fix, run 2 | 8.02 / 0.76 | 92.53 / 94.03 |
| Software RA initialization only, run 1 | 28.78 / 34.88 | 92.73 / 94.03 |
| Software RA initialization only, run 2 | 29.22 / 35.57 | 93.22 / 94.03 |

Fixed-rate diagnostic tests also improved throughput, but fixed rates are not
retained. Earlier EDCA/AMPDU/timing experiments were removed before the final
two runs. Upload-loaded latency fell from about 1061–1233 ms to 77–97 ms.
Both final speedtests reported zero packet loss. Internet speedtests include
server/path variability and do not replace local-peer radio throughput tests.

The final artifact SHA256 is
`2bf99807911ae6805722523171db4de4c8144185ad0daad558dde9db02665527`.
It passed 10 active + 10 passive scans (1.53–1.56 s, 4–5 BSS entries),
300/300 large pings, HTTPS200, reconnect and a subsequent scan, then 10/10
pings. TX queues drained with all 16 buffers free. Cross-build and host
transport/lifecycle contracts passed. Bounded optional tracing observed
MACID0 using descriptor rate17 (MCS5), rather than relying on the displayed
72.2 Mbps link rate. Cold boot of this new artifact is not yet validated.

### Feature evidence snapshot

| Area | Status |
| --- | --- |
| Station association, encrypted traffic, DHCP, HTTPS | Verified on test setup |
| Active/passive scans, reconnect | Verified with management-ACK failfast workaround |
| Native SDIO IRQ, networkd autoconnect across cold boot | Verified with earlier scan-fix artifact; see `latency.md` |
| Automatic-mode throughput after software RA initialization | Verified in two paired runs above |
| Rate feedback correctness across signal/rate changes | Implemented/inherited, not fully validated |
| CCX management-ACK reports | Incomplete; failfast remains enabled |
| EDCA, CAM/security offload, aggregation controls | Incomplete SCI adaptation/audit |
| Calibration, TX-power limits, 40 MHz, long soak | Unvalidated/incomplete |
| AP, P2P, monitor, power saving, WPA3 | Unvalidated; no SCI support claim |
| Warm reboot | Unresolved; cold power-on works with prior artifact |

## Follow-up: associated scan timing

Repeated tests on the installed milestone with networkd active found:

- Ordinary active `iw` scans returned `scan aborted!` with no BSS entries.
  The `iw` process exited zero even on these aborted scans, so exit status
  alone is insufficient for validation.
- Passive scans sometimes returned four BSS entries, but also aborted in a
  later repetition. Passive scanning is not a reliable workaround yet.
- Kernel logs showed `rtw_scan_timeout_handler` interleaved with repeated
  `dump_mgntframe_and_wait_ack_timeout` messages. One passive survey completion
  arrived after its watchdog had already expired.
- After a 15-second settling interval, interface-bound ping recovered to
  10/10 replies. Scanning can temporarily disrupt traffic.

Source inspection identifies a timing hypothesis: `rtw_ps_annc()` calls
`issue_nulldata(..., 3, 500)` for associated stations. Each unsuccessful ACK
wait is 500 ms, plus 500 ms between attempts: about 2.5 seconds per announcement.
The scan timeout calculation budgets channel dwell, back-operation dwell and
a fixed 2-second allowance, without explicitly accounting for these repeated
ACK waits. Missing SCI TX-report/ACK integration may therefore contribute to
scan watchdog expiry. This is a hypothesis, not a verified fix; TX-report
delivery and transition timing need instrumentation before changing behavior.

## Validated bounded-wait workaround

SCI-only timing traces confirmed 500 ms management-ACK waits occupying the
command thread before scan entry. Many originated from block-ack action retries,
not just null-data announcements. Scans had a 3400 ms watchdog and could expire
before their command ran. Missing report initialization was also found:
stock `_hal_init_tll9083xs` enables 0x44ec bits 7:6, programs 0x44ed=0x40,
and later sets 0x44f0=0xbc0f. Restoring this produced type-2 rate reports,
but no type-1 CCX reports. The non-copy SDIO receive branch now dispatches
CCX reports if any arrive instead of only logging them.

Default SCI behavior now attempts management transmission and returns `_FAIL`
immediately when ACK confirmation is requested. It does not manufacture ACK
success. Scan power-save announcements use the existing repeated, non-waiting
null-data path. This avoids both unavailable ACK waits and 500 ms retry sleeps.
The scan watchdog remains unchanged. Set `s9082_ack_failfast=0` only for report
bring-up experiments; it restores the blocking behavior and known scan failures.
`s9082_scan_trace=1` enables at most 600 metadata-only trace records per load.

Denied/unqueued SCI cfg80211 scans return `-EBUSY` rather than completing
synchronously inside the request callback. The former path could leave `iw`
waiting for an event already emitted; callers should retry a busy request.

Validation on the test box with networkd: 10 active plus 10 passive scans
completed in 1526–1569 ms, returning 4–5 BSS entries without aborts or watchdog
events. A final default-parameter build repeated all 20 scans in 1535–1563 ms,
returning 4–6 BSS entries. Reconnect followed by another scan passed. TX queues drained with all
16 buffers free. Initial post-scan traffic was 299/300 large pings and HTTPS200;
a settled repeat passed 300/300 large pings and HTTPS200. The final build passed
300/300 large pings twice and HTTPS200 twice. The exact armbian-config parsing
pipeline yielded five complete AP entries. The interactive menu itself was
not automated.

Stock evidence: initialization
`ev_05e6955a54ac243924cd1fc235c77f7340d84f3a137c1cb5435f521a4830eb9b`;
CCX result handler
`ev_d994d190ab3c679b0a4196f546f509f9bb3070c1025c0e956be273972a850557`.
