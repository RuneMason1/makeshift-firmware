# MakeShift Release Lineage

Mirrored in `makeshift-firmware`, `makeshift-ctrl`, `makeshift-msg`, and
`makeshift-serial`. Update all copies before every coordinated release push.

## Current unpaired state

| Component | Branch | Commit |
| --- | --- | --- |
| Firmware | `stabilization/led-ui-hardening` | `e02ee82035e7d3e14144106ba52d1692e75f8a2e` |
| Ctrl | `modernize/ctrl-2026` | `be2bab84986f55e2ebeffd0d505e23708d15cbde` |
| Msg | `main` | `1f941dbf7235951b060f3be7d2fc9e53cb78771d` |
| Serial | `agent/binary-display-packets` | `2ba800e00303601a0995de582b7c1d1b5f63fcc4` |
| Agent companion | `systray-widget/main` | `63141a3` |

Current local/flashed firmware state:

- Local source is an uncommitted keyed-cache investigation on top of `e02ee82`.
- Flashed device image is the hardware-approved rollback checkpoint
  `2026-08-13-approved-led-ui-stabilization/firmware.hex`.
- Flashed SHA-256:
  `0EF975629F22743B7F33A7CCFBB2A79111D2CECE7734F89D1919ACDD421416C2`.

Status: unpaired. The archive checksum still needs correction, the private cue
manifest is not recorded, and the required physical cold-boot validation is
pending.

## Release rule

Before every coordinated push, update this file in all four repositories with
the exact component commits, agent companion commit, published HEX path and
SHA-256, private cue-manifest SHA-256, protocol result, and cold-boot result.
Then create the same annotated `lineage/<id>` tag in all four repositories.

## 2026-09-07 Keyed Cache LED Regression

The experimental keyed-cache firmware was flashed twice, including a physical
ten-second USB power cycle after each upload. Both attempts produced the same
hardware regression: the startup LED sequence failed and physical LED 3
remained green. No host LED command was logged at either failure.

The approved August 13 checkpoint was then flashed and passed the same
cold-boot test: the startup sequence and LED 3 returned to normal.

Independent same-toolchain builds established these memory facts:

- LED-good `e02ee82` direct-art build: RAM2 variables `255,904`, free
  `268,384` bytes; seven 80x80 RGB565 cache buffers (`89,600` bytes).
- Keyed-cache candidate: RAM2 variables `332,704`, free `191,584` bytes;
  thirteen buffers (`166,400` bytes).
- The added `76,800` bytes reside in OCRAM/DMAMEM after the WS2812
  `displayMemory`; linker symbols show no direct buffer overlap.

Conclusion: cache firmware is unsafe on physical hardware despite no proven
address overlap. Do not flash any keyed-cache build until an isolated A/B
candidate passes physical cold-boot LED validation. The restored firmware
does not advertise cache protocol v2, so Ctrl automatically uses its legacy
direct-art transport.

### Narrowed A/B boundary

The original failed candidate changed two startup-layout inputs beyond the
packet handlers: it raised `MEDIA_CACHE_SLOTS` from 7 to 13 and enlarged the
SLIP decoder from 256 to 1,152 bytes. The host no longer needs either change:
its cache chunks are 224 bytes plus a four-byte offset.

The conservative keyed-cache candidate restores `SLIPPacketSerial` and seven
RGB565 buffers. Its linker output matches `e02ee82` for the relevant startup
symbols and totals: `packetSerial` at `0x20008e50`, WS2812 drawing/display
buffers at `0x200331c0` and `0x20225800`, cache pixels at `0x20225920`, and
RAM2 `255,904` used / `268,384` free. Its tested build SHA-256 is
`A8CA3732ED54A127488E70B6C42DF0D0D9E9E8789BC4CBE7D40D442321C1A7F0`.

This isolates the prior regression to the enlarged cache/decoder resource
layout, with the 13-buffer DMA allocation as the primary suspect. It remains
an unflashed A/B candidate until explicitly approved.

### Reproducible build record

The conservative candidate was rebuilt locally on 2026-09-06 using
`py -m platformio run`, not an upload command. Resolved tooling was PlatformIO
Core 6.1.19, Teensy platform 4.18.0, Teensyduino 1.58, `tool-teensy` 1.62.0,
and GCC ARM 11.3.1. The library resolver selected Adafruit GFX 1.12.6,
Adafruit BusIO 1.17.4, PacketSerial 1.4.0, Teensy TimerInterrupt 1.3.0, and
TeensyID 1.4.0.

`platformio.ini` currently contains compatible-version ranges for several
libraries, not a lockfile. This dependency resolution must be treated as part
of the candidate identity during future comparisons; do not attribute a new
binary or LED behavior solely to source changes until the resolved set matches.

### Protocol 3 candidate

The current unflashed candidate adds correlated cache acknowledgements without
changing the conservative seven-slot DMA layout or the 256-byte SLIP decoder.
`CACHE_FILE_BEGIN`, `CACHE_FILE_COMMIT`, and `CACHE_FILE_BIND` reply with
`ACK, requestPacketType`; Ctrl waits for each reply or matching `ERROR` before
continuing dependent cache work. The 2026-09-06 build SHA-256 is
`77DC59F98D2D66C106610F081454F768E7D69F5EC21EF4D88544828E950C1CE5`.
It uses RAM2 `255,904` / `268,384` free and is not approved for flashing until
the required physical cold-boot LED sequence A/B test.
