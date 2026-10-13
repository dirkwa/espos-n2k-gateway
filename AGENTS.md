# espos-n2k-gateway

NMEA 2000 to Wi-Fi gateway firmware on espOS: CAN frames off the bus, out
over TCP in candump format for signalk-server, and back onto the bus. The bus
code is espOS's `espos_n2k` component (the `espos/` submodule); this repo
picks the pins, the components and the partition table per chip.

## Rules

- **Fixes to the bus, WiFi, config, OTA or the web UI belong in espOS**, with
  a host test where the logic is testable. Merge there first, then bump the
  submodule here. A workaround here names the espOS issue and goes when the
  bump lands.
- **The gateway decodes nothing.** PGN decoding stays in canboat on the server.
- **Pins come from the `can` settings**, with per-chip defaults in
  `main/Kconfig.projbuild`. A pin the flash or PSRAM uses must stay refused:
  routing CAN onto it crashes the device at every boot.
- **Partition tables are fixed at the first USB flash.** OTA never rewrites
  them, so a change reaches only newly flashed boards.
- **Build through `scripts/build.sh`** (espOS's locked, nice'd wrapper), one
  build directory per target. Zero warnings.
- **Never commit boat configuration** (WiFi credentials, server addresses) or
  the signing key. Site config lives in NVS.

## Conventions

- Angular conventional commits and PR titles, `type(scope): subject`, subject
  lower case, at most 50 characters. PR titles become the release notes.
- Branch names `feat/…`, `fix/…`, `docs/…`; never commit to `main`.
- No AI attribution anywhere: no Co-Authored-By, no "Generated with".
- No version bumps or tags by hand; release-please cuts releases.
- Comments say why, not what.
