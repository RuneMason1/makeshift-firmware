# Ctrl Asset Cache Protocol

## Purpose

The firmware exposes bounded RGB565 storage to Ctrl as opaque asset keys. Ctrl
owns names, providers, and carousel policy; firmware owns memory limits,
validation, and rendering bindings. This prevents one carousel's item index
from selecting another carousel's artwork.

## Current capability

The conservative, unflashed candidate reserves seven 80x80 RGB565 images in
`DMAMEM`:

- capacity: 89,600 bytes
- maximum image: 80x80 RGB565 (12,800 bytes)
- replacement: least-recently-used entry when the cache is full
- build margin: RAM2 uses 255,904 bytes with 268,384 bytes free

This layout matches the LED-good direct-art baseline. The cache is host-visible
as bytes. Its internal allocation is not a
carousel contract and must not be exposed to cues or providers.

## Packets

The legacy collection packets `7`, `8`, and `9` remain supported for rollback
compatibility. New cache packets are additive:

| Packet | Value | Payload |
| --- | ---: | --- |
| `CACHE_FILE_BEGIN` | 27 | `key:u32, width:u16, height:u16` |
| `CACHE_FILE_CHUNK` | 28 | `byteOffset:u32, rgb565Bytes` |
| `CACHE_FILE_COMMIT` | 29 | empty |
| `CACHE_FILE_BIND` | 30 | `key:u32, collectionItemIndex:u8` |

`CACHE_FILE_BIND` never copies pixels. It only associates an already committed
asset with one item in the currently active collection. Opening another
collection clears those associations but keeps keyed assets resident.

Cache protocol 3 sends a correlated acknowledgement for `BEGIN`, `COMMIT`,
and `BIND`: `ACK, requestPacketType`. Ctrl must wait for that acknowledgement
before it sends dependent work. `ERROR` already includes the rejected request
packet type and is treated as a rejected pending acknowledgement. Chunk writes
remain ordered and bounded but are not individually acknowledged.

## Throughput work

USB bus speed is not the current limiting factor. The Ctrl adapter uses a
conservative 224-byte data payload so its four-byte offset fits under the host
wrapper's proven 240-byte packet-body limit, preserving the LED-good 256-byte
SLIP decoder. It presently spaces chunk writes by 3 ms because PacketSerial
does not expose host write backpressure. Protocol 3 adds state-transition ACKs
without enlarging that decoder or the DMA cache allocation. Ctrl enables the
keyed path only after receiving `protocol=3`; older firmware stays on legacy
direct art. Hardware timing validation is still required before claiming a
specific end-to-end latency. No firmware flash is implied by this document.
