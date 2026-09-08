# Agent/Firmware Protocol Lineage

## Current paired cache release

The confirmed flashed baseline is firmware `9e10060` on
`stabilization/led-ui-hardening`, paired with Ctrl Core `a03a7be` on
`modernize/ctrl-2026`. It explicitly implements keyed cache protocol 4:

- Packet 27-30 provide `CACHE_FILE_BEGIN`, `CACHE_FILE_CHUNK`,
  `CACHE_FILE_COMMIT`, and `CACHE_FILE_BIND` with correlated acknowledgements.
- The device reports cache protocol 4 and 89,600 bytes of cache capacity.
- Firmware `8932a12` resets the active collection input session on Ctrl
  disconnect, preventing a stale session from consuming physical controls.
- Firmware `9e10060` and Ctrl `f57978b` added packet 32,
  `COLLECTION_PRESENTATION`, so a cue supplies its idle and activation labels.
- Firmware `74693bc` and Ctrl `8a10a66` are the next unflashed Phase 3
  candidate. It stages collection lists, cache replacement, and runtime
  manifests until commit, with bounded cancellation paths and acknowledged
  runtime-asset synchronization.

## Historical direct-art baseline on 2026-09-06

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
