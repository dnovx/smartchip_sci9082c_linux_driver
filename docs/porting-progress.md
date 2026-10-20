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

- Associated direct scan aborts: reproduce using `iw dev wlan0 scan` and
  retain exit status/stderr. armbian-config parses this direct scan.
- Generic RTL8188E accesses still produce invalid0xeaea values: CAM/security,
  RXDMA reset checks, EDCA and management ACK handling need evidence-backed
  adaptation. Successful encrypted traffic does not prove hardware CAM works.
- Warm reboot did not return; cold power-on works. Cause unresolved.
- Calibration, power management, extended bandwidth/features, long-duration
  soak and TCP throughput require independent validation.

The historical “98%” was an engineering estimate, not a compatibility score.
