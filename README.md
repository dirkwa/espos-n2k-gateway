# espos-nmea-gateway

An NMEA to Wi-Fi gateway for ESP32 boards, built on
[espOS](https://github.com/signalk-espOS/espOS). NMEA 2000 works today; NMEA
0183 over a serial port is planned as an option beside it, so one board serves
whichever bus the boat has.

On NMEA 2000 it reads every CAN frame off the boat's backbone and streams it
over TCP in candump format to
[signalk-server](https://github.com/SignalK/signalk-server), which decodes it
with canboatjs. Frames the server sends back go onto the bus.

The gateway decodes nothing. PGN decoding changes far more often than firmware
should, so the server owns it and the device stays a pipe: a small, cheap tap
on the backbone, wherever the cable runs, with the computer below decks on
Wi-Fi.

## What you need

* **An ESP32 board** with one of the chips below. The ESP32, S3, C3 and C6
  images need 4 MB of flash; the C5 and P4 images use espOS's 16 MB table,
  which the Waveshare boards they are released for carry.
* **A CAN transceiver** between the board and the bus. The ESP32's CAN
  controller (TWAI) speaks 3.3 V logic and cannot drive the bus's differential
  pair. An SN65HVD230 (or SN65HVD231/232) module runs on 3.3 V and wires as
  shown below. A TJA1051 needs 5 V on VCC; use the TJA1051T/3 variant, whose
  VIO pin takes 3.3 V so its logic matches the ESP32's. Some boards have a
  transceiver on board (the SH-ESP32, for example).
* **A drop cable** to the backbone: an NMEA 2000 T-piece and a cable with a
  Micro-C connector, or a field-attachable connector.
* **Power.** The bus carries 12 V on its NET-S/NET-C pins, but the board needs
  3.3 V or 5 V: use a 12 V to 5 V converter, or power the board separately.

## Wiring

```
 ESP32                 transceiver            NMEA 2000 drop cable
 ─────                 ───────────            ────────────────────
 CAN TX pin  ───────►  TXD (CTX)
 CAN RX pin  ◄───────  RXD (CRX)
 3V3         ───────   VCC  (SN65HVD230)
 GND         ───────   GND  ──────────────    NET-C (0 V, black)
                       CANH ──────────────    NET-H (white)
                       CANL ──────────────    NET-L (blue)
```

The default pins per chip are below. Any other free GPIO works: set them on
the **NMEA 2000 bus** settings page of the device's web UI and restart it. A pin
the flash or PSRAM uses is refused, and the log says why.

| Chip | Default TX | Default RX | Where the default comes from |
|---|---|---|---|
| ESP32 | GPIO 32 | GPIO 34 | the SH-ESP32 board, which has the transceiver on board |
| ESP32-C5 | GPIO 4 | GPIO 5 | free on the Waveshare ESP32-C5 kit |
| ESP32-S3, -C3, -C6 | GPIO 4 | GPIO 5 | free on the common dev kits |
| ESP32-P4 | GPIO 20 | GPIO 21 | the Waveshare ESP32-P4 boards |

Two things go wrong most often, and both look like a software fault:

* **Termination.** A CAN bus needs 120 Ω at each end. On a boat the backbone
  already has both terminators, so a drop cable adds none; do not add a third.
  On a bench with no backbone, put 120 Ω across CANH and CANL at each end, or
  nothing is received. Many transceiver modules carry a 120 Ω resistor of their
  own: remove it (or open its jumper) when the module hangs off a real
  backbone.
* **TX and RX swapped.** The board's TX pin goes to the transceiver's TXD.

Keep CAN off the chip's strapping pins, above all for RX: the transceiver
holds RX high from the moment it has power, so it decides that pin's level
while the chip boots, before the firmware runs. On the ESP32, GPIO 12 high at
reset selects 1.8 V flash and a board with 3.3 V flash then does not start at
all; GPIO 0, 2, 5 and 15 change how it boots. The other chips have their own
strapping pins, listed in each chip's datasheet. No setting can undo a board
that does not boot, so this is one to check before wiring.

The bus runs at 250 kbit/s, which NMEA 2000 fixes; there is nothing to set.

## Setup

1. **Flash the firmware.** The Signal K espOS manager flashes it from the
   browser over USB; or download the merged image for your chip from the
   [releases](../../releases) and write it at offset 0 with esptool.
2. **Join Wi-Fi.** On first boot the device raises an access point named
   `espOS-xxxx`. Join it, pick the boat's network, and the device's web UI is
   at `http://<device>/` from then on.
3. **Add it to signalk-server.** Server → Data Connections → Add:
   * type **NMEA 2000**, source the **IP gateway** (n2k-ip-gateway-canboatjs)
   * format **candump3**
   * Host: the device's address. Port: **2599**.

   The device also advertises `_sensesp-n2k._tcp` over mDNS, the name the
   SensESP NMEA 2000 gateway used, so clients that browse for it find this one
   too.

To keep the gateway from ever sending onto the bus, turn off **Send to the
bus** on the **NMEA 2000 bus** settings page. It still acknowledges frames, as every node on a CAN bus does, but never sends one of its own.

## Is it working?

`http://<device>/api/v1/n2k` reports the bus without a serial cable:

| What it shows | What it means |
|---|---|
| `frames: 0`, `errors: 0` | nothing is transmitting, or the gateway is not on the bus: check the cable and the transceiver's power |
| `frames: 0`, errors climbing | the gateway is on the bus and cannot read it: CANH/CANL swapped, a broken pair, or termination missing |
| frames climbing | it works |

The device log, under **Logs** in the web UI, says which pins it used and
whether the bus is live.

## Build

Needs ESP-IDF exactly `v6.0.3` (pinned in `.idf-version`, equal to the espOS
submodule's pin) and nothing else: the espOS web UI is a committed bundle.

```sh
git clone --recursive https://github.com/dirkwa/espos-nmea-gateway
cd espos-nmea-gateway
. ~/esp-idf-v6.0.3/export.sh

# esp32c5 here; esp32 / esp32s3 / esp32c3 / esp32c6 / esp32p4 the same way.
scripts/build.sh -B build-esp32c5 -DSDKCONFIG=build-esp32c5/sdkconfig -DIDF_TARGET=esp32c5 build
idf.py -B build-esp32c5 -p /dev/ttyACM0 flash monitor
```

`scripts/build.sh` forwards to espOS's build wrapper: one build at a time per
machine, half the cores, `nice`/`ionice`.

The default pins are Kconfig options (`main/Kconfig.projbuild`), for a build
made for one board. The sdkconfig is assembled by espOS
(`espos/docs/development.md`): its defaults, then this project's
`sdkconfig.defaults*`, then a git-ignored `sdkconfig.local`. Defaults reach a
fresh sdkconfig only, so delete `build*/sdkconfig` after changing them.

The bus code lives in espOS (`espos/components/espos_n2k`); this project picks
the pins and wires the receiver, the transmitter and the candump server
together (`main/main.cpp`).

## Releases

release-please watches `main`, opens a `chore: release <version>` pull
request, and merging it tags the release and builds, signs and attaches the
images for every chip (`release-firmware.yml`).

The repository needs:

* **Settings → Actions → General → "Allow GitHub Actions to create and approve
  pull requests"**, with read/write workflow permissions, or release-please
  cannot open its pull request.
* **`N2K_GATEWAY_SIGNING_KEY_PEM`**, the app-signing key (the name dates from
  before the rename and stays, so existing keys keep working). A release built
  without it uses a throwaway key: it installs over USB and then refuses every
  OTA.
* Optional: the `DOCKERHUB_USERNAME` variable and the `DOCKERHUB_TOKEN` secret,
  so builds pull the ESP-IDF image logged in rather than under the anonymous
  rate limit.
* Optional: `RELEASE_APP_PRIVATE_KEY`, for the espOS tracking PRs and to tell
  the registry about a new release straight away.

## Layout

```
espos/               espOS submodule: WiFi, config, web UI, SignalK, OTA,
                     and components/espos_n2k, where the bus code lives
main/main.cpp        pins from the settings, receiver + transmitter + server
main/config/can.json the "NMEA 2000 bus" settings page
main/Kconfig.projbuild  the default pins per chip
sdkconfig.defaults*  only what differs from espOS
CMakeLists.txt       which espOS partition table each chip gets (4 or 16 MB)
scripts/build.sh     forwards to espos/scripts/build.sh
```

## License

Apache License 2.0 (`LICENSE`).
