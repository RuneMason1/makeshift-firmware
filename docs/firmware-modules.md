# MakeShift Firmware Modules

## Purpose

This document describes how firmware features should be added and used going
forward.

The target architecture is:

- `makeshift-firmware`: generic device runtime
- `makeshift-ctrl`: generic host/runtime coordinator
- cues: private, user-specific behavior and integrations

Firmware modules must be reusable. If a feature only makes sense as one
person's custom behavior, it should not live in firmware.

## Design Rules

Every firmware module should satisfy these rules:

- It exposes a reusable primitive, not a personal workflow.
- It can be described without naming a specific app, account, or user.
- It communicates through generic messages and state, not one-off behavior.
- It can be driven by ctrl or cues without recompiling the firmware for each
  user.

Good examples:

- collection/card view
- image cache
- glyph overlay
- status panel
- LED pattern engine
- boot sequence engine
- screen zone renderer

Bad examples:

- "Mason's Pandora widget"
- "GoXLR mute button logic"
- "Steam launch carousel"

Those belong in ctrl or cues.

## Module Boundaries

### Firmware should own

- hardware input scanning
- LED output primitives
- display rendering primitives
- generic local state machines
- generic caches
- boot/connection/default visual behavior
- transport-safe packet handling

### Ctrl should own

- asset discovery and preparation
- preferences and profiles
- runtime orchestration
- mapping device events to cue actions
- generic feature coordination across modules

### Cues should own

- app-specific automation
- personal mappings
- private integrations
- user-specific assets and workflows

## Current Module Direction

### 1. Collection View

The current "game carousel" is being generalized into a collection/card view.

The end state should be:

- firmware knows about collections, items, selection, activation, and card art
- firmware does not know about Steam or games
- ctrl decides what the collection represents
- cues decide what activation does

### 2. General Media Cache

The current artwork cache should become a shared cache for any module that
needs device-side images.

Potential consumers:

- collection/card view
- splash assets
- cue-loaded zone art
- glyph panels
- future media/album art

As of August 12, 2026, the first extraction pass has started in firmware via
`mkshft_media_cache`, which is intended to become the reusable device-side
image cache primitive.

The cache should not be named or scoped around a single feature.

### 3. Visual Preferences

Visual preferences are a good example of a reusable firmware feature.

Current reusable preferences already include:

- home splash image selection
- LED active color
- USB connected color
- USB disconnected color

These are generic device visuals, not user-specific cue behavior.

### 4. Overlay Glyphs

Centered overlay glyphs should be treated as a reusable module, not a
media-transport special case.

They can be used for:

- previous / play-pause / next
- mute / unmute
- future generic transport or status confirmations

The current wire packet is still named `ACTION_GLYPH` for compatibility, but
the rendering primitive is intentionally broader than media actions.

Recommended public asset names for cue-driven overlays:

- `media.previous`
- `media.play-pause`
- `media.next`
- `media.mute`
- `media.unmute`

### 5. Screen Zones

Features request a semantic screen zone; they do not own display coordinates:

- `SPECIAL` (`0`) is an exclusive full-screen experience such as a collection carousel.
- `LOWER_LEFT` (`1`) is a persistent status badge, currently used by bedroom lighting.
- `LOWER_RIGHT` (`2`) is a persistent status badge, currently used by GoXLR.
- `UPPER` (`3`) is the scrolling ticker/banner region.
- `CENTER` (`4`) is the temporary glyph/confirmation overlay.

Legacy names remain aliases at the same numeric values. New modules should use
the canonical names above and render through the zone owner rather than adding
feature-specific coordinates.

## How To Add A New Firmware Module

Before adding code, ask:

1. Is this a reusable primitive?
2. Could another user use it without rewriting firmware?
3. Could cues drive it generically?
4. Does it belong in firmware instead of ctrl or cues?

If the answer to any of those is "no", stop and move the feature upward into
ctrl or cues.

If the feature does belong in firmware, implement it in this order:

1. Define the generic capability.
2. Define the generic state and rendering behavior.
3. Define the generic protocol surface.
4. Add compatibility wrappers if older ctrl code still uses legacy names.
5. Document how ctrl and cues are expected to consume it.

## Naming Guidance

Prefer names like:

- `collection`
- `item`
- `card`
- `asset`
- `cache`
- `zone`
- `overlay`
- `panel`
- `selection`
- `activation`

Avoid new names like:

- `game`
- `steam`
- `pandora`
- `goxlr`
- a specific user name

Legacy names may remain temporarily for compatibility, but new work should
prefer generic terminology.

Current compatibility examples:

- runtime/public code may speak in `collection-view` while legacy firmware
  compatibility still uses `GAME_*` packet ids
- overlay glyphs are a generic module even though the current wire packet name
  remains `ACTION_GLYPH`

## Current Compatibility Strategy

As of August 12, 2026:

- internal firmware APIs have started gaining generic collection-oriented
  wrappers
- legacy `GAME_*` packet names still exist for compatibility
- legacy emitted strings like `GAME_SELECT` still exist for compatibility

This is intentional. Behavior should stay stable while the architecture is
generalized in controlled steps.

## Next Refactors

- move the current card art storage into a general reusable cache module
- rename protocol concepts from `game` to `collection/item/card`
- remove app-specific activation semantics from firmware
- document each reusable module as it becomes public API
