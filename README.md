# Smartchip SCI9082C SDIO Linux driver

Experimental SCI/Smartchip port for **SDIO 02e7:9082**, built on the inherited
RTL8188E driver. The working milestone is station connectivity on an Amlogic
S905X AArch64 box running **6.18.44-ophub**. Other kernels/boards are untested.

## Tested status

- Probe, firmware boot, read-only efuse, 2.4GHz channel selection (20MHz).
- WPA-PSK/CCMP association, DHCP, internet traffic and HTTPS.
- 1,500/1,500 1000-byte pings over five minutes with HT enabled; queues drain.
- Driver autoload and network autoconnect after physical power-on.
- Netplan/networkd and NetworkManager connectivity tested independently.
- Associated active/passive scans: 10 consecutive scans each passed after
  bounding unavailable management-ACK waits; see the scan follow-up report.

**Known gaps:** warm reboot failed on the test box; generic register and
CAM/key-table paths need further adaptation. Management ACK confirmation is
unavailable: frames are transmitted but ACK-dependent operations fail promptly
instead of blocking the command queue. This is a workaround, not working CCX.
AP/P2P/monitor, WPA3, power saving, 40MHz, throughput and long soak are not
validated. A reported 72.2Mbps link is not a throughput measurement.

## Build and install

You need matching kernel development headers, make, a suitable C compiler,
binutils, kmod, Python3 and pyelftools. For example, install the distribution's
build prerequisites and **headers matching your actual running kernel**.
The ophub kernel may require headers/toolchain from its own distribution.
The tested kernel requires compiler options unsupported by GCC13.

Firmware is **not distributed here**. Supply your own supported stock
`9082xs.ko`; see [provenance](docs/provenance.md).

```sh
git clone https://github.com/dnovx/smartchip_sci9082c_linux_driver.git
cd smartchip_sci9082c_linux_driver
python3 -m venv .venv
. .venv/bin/activate
pip install pyelftools
python3 tools/extract-firmware.py /path/to/stock-9082xs.ko
sh tools/check-system.sh
sh tools/build.sh
sudo sh tools/install.sh --autoload
sudo modprobe 8189es
```

Cross-build: set `KSRC=/path/to/prepared/target/headers ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-` when invoking `sh tools/build.sh`.
Headers must include matching configuration/Module.symvers and working host
build helpers. Never substitute unrelated headers merely to make it compile.
The wrapper selects the tested AArch64 station-only configuration.
Host-only contract checks: `python3 tools/test-contracts.py` (requires GCC).

On the tested S905X, a missing board DT `cap-sdio-irq` property caused interrupt
polling and measurable extra latency. See [latency findings](docs/latency.md)
for paired measurements and the board-specific verification limits.

Configure your own SSID/password through `armbian-config`, Netplan or your
existing network manager. The installer does not change network backends.
Busy/denied scan requests return `-EBUSY`; retry after settling. Keep Ethernet/console access during
experiments. Validate with `sh tools/verify-install.sh wlan0`.

Uninstall: `sudo sh tools/uninstall.sh`. Kernel updates require rebuilding;
DKMS is not provided. See [progress and limitations](docs/porting-progress.md).

## Attribution and licensing

Preserves inherited source/history and GPL notices. Local base commit:
`325748e04685d5a88b435000fc58f5e4eee8ebe6`, branch
`rtl8188e_usb_sdio_5.13.3-17-test` from the chalesyu RTL8188E tree.
Stock-module contract recovery informed the SCI port; this is not a claim
that the recovered proprietary source was obtained or that all hardware
behavior is equivalent. Firmware has separate, unestablished redistribution
terms and is excluded. Existing source license notices remain authoritative.

---

## Historical upstream README (Realtek features below are not SCI test claims)

-----

## rtl8188eu (8188eu.ko)

## Realtek RTL8188EU Wireless Lan Driver for Linux

- v5.13.3 (20210615)
- Based on EDIMAX EW-7811Un V2 Linux Driver (Version : 1.0.1.3) 2021-10-01
- Support Kernel: 3.8 - 5.4 (Realtek)
- Support up to Kernel 6.2

## Specification

- Supported interface modes:
  * IBSS
  * managed
  * AP
  * monitor
  * P2P-client
  * P2P-GO
- Packet injection
- TX power control
- LED control
- Power saving control
- Driver debug log level control
- REGD source selection
- Security:
  * WEP 64/128-bit, WPA, WPA2, and WPA3

## Supported adapters

- TP-Link TL-WN722N V2/V3 150Mbps High Gain Wireless USB Adapter
- TP-Link TL-WN727N V5.20 150Mbps Wireless N USB Adapter
- TP-Link TL-WN725N V3 150Mbps Wireless N Nano USB Adapter
- EDIMAX EW-7811Un V2 N150 Wi-Fi 4 Nano USB Adapter
- ASUS USB-N10 Nano B1 USB Adapter Wireless-N
- D-Link DWA-125 Wireless N 150 USB Adapter(rev.D)
- D-Link DWA-123 Wireless N 150 USB Adapter(rev.D)
- D-Link GO-USB-N150 Wireless N 150 Easy USB Adapter(rev.B)
- D-Link DWA-121 Wireless N 150 USB Adapter(rev.B)
- Realtek RTL8188EU Wireless LAN 802.11n USB 2.0 Network Adapter
- Realtek RTL8188ETV Wireless LAN 802.11n USB 2.0 Network Adapter
- 802.11bgn Mini Wireless LAN USB2.0 Adapter
- ELECOM WDC-150SU2M Wireless Adapter
- Sitecom WLA-1100 V2 Wi-Fi USB adapter N150
- MERCUSYS MW150US V2 N150 Wireless Nano USB Adapter
- Rosewill RNX-N150NUB N150 Wireless Nano USB Adapter

## Tested Linux Distros and Tools

Successful tested injection `aireplay-ng -9 <interface>`, deauthentication `aireplay-ng -0 1 -a <bssid> <interface>`.

NO `fixed channel 1` issue.

Successful hcxdumptool driver test `hcxdumptool -i <interface> --check_driver` and capture packets `hcxdumptool -i <interface> -o test.pcapng --filterlist_ap=targets.lst --filtermode=2 --enable_status=15` `(hcxdumptool -i <interface> -o test.pcapng --filterlist=targets.lst --filtermode=2 --enable_status=15)`.

Connecting to AP with WPA3 (WPA3-SAE) authentication method (IMPORTANT: Test distro Ubuntu 22.04 kernel 5.15 - NetworkManager 1.36.4 nmcli tool, version 1.36.4. Otherwise use wpa_supplicant).

Start WPA3 (WPA3-SAE) SoftAP.

TX power control tested with SoftAP mode and OpenWrt firmware (OpenWrt 21.02.0 r16279-5cc0535800, Channel Analysis Feature).

- Ubuntu 22.04 (kernel 5.19)
  * Aircrack-ng 1.6
  * hcxdumptool 6.2.6
  * wpa_supplicant v2.11-devel-hostap_2_10-309-gc3d389b72
  * hostapd v2.11-devel-hostap_2_10-309-gc3d389b72

<details>
  <summary>
    Ubuntu 22.04 (kernel 5.18)
  </summary>
  
  * wpa_supplicant v2.11-devel-hostap_2_10-309-gc3d389b72
  * hostapd v2.11-devel-hostap_2_10-309-gc3d389b72
  * Aircrack-ng 1.6
  * hcxdumptool 6.2.6
</details>

<details>
  <summary>
    Ubuntu 21.10 (kernel 5.17)
  </summary>
  
  * hcxdumptool 6.2.4
  * Aircrack-ng 1.6
  * wpa_supplicant v2.11-devel-hostap_2_10-151-g3085e1a67
  * hostapd v2.11-devel-hostap_2_10-151-g3085e1a67
</details>

<details>
  <summary>
    Ubuntu 21.10 (kernel 5.16)
  </summary>
  
  * wpa_supplicant v2.10-devel-hostap_2_9-2398-g8a54c252a
  * hostapd v2.10-devel-hostap_2_9-2398-g8a54c252a
  * Aircrack-ng 1.6
  * hcxdumptool 6.2.4
</details>

<details>
  <summary>
    Ubuntu 11.10 (kernel 3.0)
  </summary>
  
  * Aircrack-ng 1.5.2
  * hcxdumptool 4.2.1
</details>

<details>
  <summary>
    Ubuntu 12.04 (kernel 3.4)
  </summary>
  
  * Aircrack-ng 1.5.2
  * hcxdumptool 4.2.1
</details>

<details>
  <summary>
    Ubuntu 12.10 (kernel 3.5)
  </summary>
  
  * Aircrack-ng 1.5.2
  * hcxdumptool 4.2.1
</details>

<details>
  <summary>
    Ubuntu 13.04 (kernel 3.8)
  </summary>
  
  * Aircrack-ng 1.5.2
  * hcxdumptool 5.1.4
</details>

<details>
  <summary>
    Ubuntu 21.04 (kernel 5.11)
  </summary>
  
  * wpa_supplicant v2.10-devel-hostap_2_9-2374-g9ef8491d9
  * Aircrack-ng 1.6
  * hcxdumptool 6.1.6
</details>

<details>
  <summary>
    Ubuntu 21.10 (kernel 5.13)
  </summary>
  
  * Aircrack-ng 1.6
  * wpa_supplicant v2.10-devel-hostap_2_9-2398-g8a54c252a
  * hostapd v2.10-devel-hostap_2_9-2398-g8a54c252a
</details>

<details>
  <summary>
    Kali Linux 2019.1a (kernel 4.19)
  </summary>
  
  * Aircrack-ng 1.5.2
  * hcxdumptool 5.1.7
  * wpa_supplicant v2.10-devel-hostap_2_9-2433-g8d881d942
</details>

<details>
  <summary>
    Kali Linux 2020.3 (kernel 5.9)
  </summary>
  
  * Aircrack-ng 1.6
  * hcxdumptool 6.1.4
  * wpa_supplicant v2.10-devel-hostap_2_9-2379-g4775a5f82
</details>

<details>
  <summary>
    Kali Linux 2021.3a (kernel 5.14)
  </summary>
  
  * Aircrack-ng 1.6
  * hcxdumptool 6.1.6
  * wpa_supplicant v2.10-devel-hostap_2_9-2433-g8d881d942
</details>

<details>
  <summary>
    Ubuntu 21.10 (kernel 5.15)
  </summary>
  
  * Aircrack-ng 1.6
  * hcxdumptool 6.2.4
  * wpa_supplicant v2.10-devel-hostap_2_9-2398-g8a54c252a
  * hostapd v2.10-devel-hostap_2_9-2398-g8a54c252a
</details>

## HOW TO

### Install

Download source:

```
git clone https://github.com/ivanovborislav/rtl8188eu.git
cd rtl8188eu
```

Install missing packages:

```
sudo apt-get install bc build-essential
```

Install linux headers:

```
sudo apt-get install linux-headers-$(uname -r)
```

or

```
apt-cache search linux-headers
sudo apt-get install linux-headers-5.14.0-kali4-amd64 (for example)
apt-cache search linux-image
sudo apt-get install linux-image-5.14.0-kali4-amd64 (for example)
```

Compile:

```
make
sudo make install
```

or

```
chmod +x install.sh
./install.sh -i
```

IMPORTANT: Prevent loading old driver r8188eu.

```
echo "blacklist r8188eu" > /etc/modprobe.d/realtek.conf
```

Raspberry Pi:

Edit `Makefile`:

Ln145 - CONFIG_PLATFORM_I386_PC = `y` to CONFIG_PLATFORM_I386_PC = `n`

Ln146 - CONFIG_PLATFORM_RPI_ARM = `n` to CONFIG_PLATFORM_RPI_ARM = `y` for ARM

or

Ln147 - CONFIG_PLATFORM_RPI_ARM64 = `n` to CONFIG_PLATFORM_RPI_ARM64 = `y` for ARM64

### DKMS installation
```
sudo git clone "https://github.com/ivanovborislav/rtl8188eu.git" /usr/src/rtl8188eu-5.13.3
sudo dkms add -m rtl8188eu -v 5.13.3
sudo dkms autoinstall
dkms status
```
or
```
sudo git clone "https://github.com/ivanovborislav/rtl8188eu.git" /usr/src/rtl8188eu-5.13.3
sudo dkms add -m rtl8188eu -v 5.13.3
sudo dkms build -m rtl8188eu -v 5.13.3
sudo dkms install -m rtl8188eu -v 5.13.3
dkms status
```

### Monitor mode

```
sudo airmon-ng check kill
sudo ip link set <interface> down
sudo iw dev <interface> set type monitor
sudo ip link set <interface> up
```

### Managed mode

```
sudo ip link set <interface> down
sudo iw dev <interface> set type managed
sudo ip link set <interface> up
sudo systemctl restart NetworkManager (sudo service network-manager restart)
```

### TX power control

Note: Set TX power before start SoftAP mode. `...set txpower fixed 3000 = txpower 30.00 dBm`.

```
sudo iw dev <interface> set txpower fixed 3000
```

### Driver options

#### Change driver options during inserting driver module

Remove (unload) a module from the Linux kernel.
```
sudo rmmod /lib/modules/$(uname -r)/kernel/drivers/net/wireless/8188eu.ko
```

Insert (load) a module into the Linux kernel.
```
sudo insmod /lib/modules/$(uname -r)/kernel/drivers/net/wireless/8188eu.ko rtw_ips_mode=1 rtw_drv_log_level=4 rtw_power_mgnt=2 rtw_led_ctrl=1
```

#### Change driver options loading from file

Create a file `8188eu.conf` containing `options 8188eu rtw_ips_mode=1 rtw_drv_log_level=4 rtw_power_mgnt=2 rtw_led_ctrl=1`.
Copy a file to `/etc/modprobe.d/` directory.

```
sudo cp -f 8188eu.conf /etc/modprobe.d
```

Power saving control.

IPS (Inactive Power Saving) Function, rtw_ips_mode=
```
0:Disable IPS
1:Enable IPS (default)
```

LPS (Leisure Power Saving) Function, rtw_power_mgnt=
```
0:Disable LPS
1:Enable LPS
2:Enable LPS with clock gating (default)
```

Driver debug log level control, rtw_drv_log_level=
```
0:_DRV_NONE_
1:_DRV_ALWAYS_
2:_DRV_ERR_
3:_DRV_WARNING_
4:_DRV_INFO_ (default)
5:_DRV_DEBUG_
6:_DRV_MAX_
```

Driver LED control, rtw_led_ctrl=
```
0:led off
1:led blink (default)
2:led on
```

Driver REGD source selection, rtw_regd_src=
```
0:Realtek defined
1:OS (default, get channel plan from OS)
```

### Connecting with wpa_supplicant

Example wpa_supplicant.conf with WPA3-Personal (WPA3-SAE).

```
ctrl_interface=/var/run/wpa_supplicant
update_config=1
country=EN
p2p_no_group_iface=1
sae_groups=19 20 21
network={
    ssid="WPA3"
    proto=RSN
    key_mgmt=SAE
    pairwise=CCMP
    group=CCMP
    ieee80211w=2
    psk="1234567890"
}
```

Now start...
```
sudo systemctl stop NetworkManager
sudo killall wpa_supplicant
sudo wpa_supplicant -B -i <interface> -c wpa_supplicant.conf
sudo dhclient <interface>
```

### Start SoftAP mode

Example hostapd.conf with WPA3-Personal (WPA3-SAE).

```
ctrl_interface=/var/run/hostapd
ctrl_interface_group=0
driver=nl80211
ssid=WPA3
country_code=EN
channel=6
beacon_int=100
dtim_period=1
fragm_threshold=2346
rts_threshold=2347
hw_mode=g
ieee80211n=1
max_num_sta=16
wmm_enabled=1
ht_capab=[HT40-][SHORT-GI-20][SHORT-GI-40][RX-STBC1][MAX-AMSDU-7935][DSSS_CCK-40]
ignore_broadcast_ssid=0

auth_algs=1
wpa=2
wpa_key_mgmt=SAE
wpa_pairwise=CCMP
rsn_pairwise=CCMP
ieee80211w=2
sae_groups=19 20 21
sae_require_mfp=1
wpa_passphrase=1234567890
```

Now start...
```
sudo killall hostapd
sudo hostapd -i <interface> hostapd.conf
```

### [Kali NetHunter Kernel module Samsung Galaxy S5](https://github.com/ivanovborislav/document/blob/main/NetHunter_Kernel_module_Samsung_Galaxy_S5.md)

## Test devices

- TP-Link TL-WN722N V3

- Linksys WRT1200AC V2
  * OpenWrt 21.02.0 r16279-5cc0535800 / LuCI openwrt-21.02 branch git-21.231.26241-422c175

```
config wifi-iface 'default_radio1'
	option device 'radio1'
	option network 'lan'
	option mode 'ap'
	option macaddr '30:23:03:XX:XX:XX'
	option ssid 'WPA3'
	option encryption 'sae'
	option key '1234567890'
	option ieee80211w '2'
```

-----
