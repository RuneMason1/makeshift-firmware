# Agent/Firmware Protocol Lineage

## Verified pair on 2026-09-06

The local firmware built and flashed from `stabilization/led-ui-hardening`
(`e02ee82035e7d3e14144106ba52d1692e75f8a2e`) is the **legacy direct-art
generation**. Its behavior is established from source, not inference:

- `MessageType` ends at `STATUS_BADGE` (26).
- Carousel artwork uses packet 7 `GAME_CARD_BEGIN`, packet 8
  `GAME_ART_CHUNK`, and packet 9 `GAME_CARD_COMMIT`.
- The packet-7 payload is `slot:u8, itemIndex:u8, width:u16, height:u16,
  titleLength:u8, title:utf8`.
- The renderer owns seven 80x80 RGB565 slots, about 89,600 bytes total.
- There is one sequential direct-art transfer at a time. A new transfer
  invalidates its destination slot before commit.
- Packet IDs 27-32 are unknown and silently ignored.

## Active agent pairing

On 2026-09-06, the active Systray `MakeShiftAgent` was updated to the legacy
direct-art adapter (source/package SHA-256
`e026d47139dc747a83efc8adf26e860e3c4f5ad69dfb76d8a7e0e5f192d161d2`). It
uses packet 7/8/9 transactions, one shared session-aware transfer lane, and
does not send host-only cache packets 29-32 to this firmware.

The prior keyed-cache/session implementation was incompatible: it treated a
serial write as residency proof and sent packets this firmware silently
ignored. That mismatch is resolved for the current legacy direct-art release,
but remains the reason a future keyed-cache protocol must be a paired firmware
and control-plane release.

## Required rule

Never infer device cache capability from agent code. Ctrl/agent must select an
artwork adapter from firmware capabilities:

1. `legacy-direct-art`: immutable session items, physical slots 0-6, correctly
   shaped direct RGB565 transactions, and a single session-aware transfer lane.
2. `keyed-cache`: only with a paired firmware that explicitly implements
   packets 29-32 plus cache commit/residency semantics.

The immediate safe stabilization path is a direct-art adapter in the current
agent/shared Ctrl core. A keyed-cache firmware redesign is a separately paired,
explicitly flashed release.
