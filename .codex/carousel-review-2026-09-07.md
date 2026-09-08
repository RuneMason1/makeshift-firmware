# Carousel cache review

Second pass: Ctrl binds resident artwork before collection commit, using generic
per-item artworkIdentity metadata. Firmware suppresses binding-time redraw while
the replacement list is being staged. Coordinator invalidates the previous
active session as soon as replacement starts. Plex preloading now references
the normalized active session items: original feed object identities did not
match session.items.indexOf and caused silent preload rejection. Delayed Plex
preload captures its session rather than adopting a newly active carousel.

Confirmed beginKeyWrite passed itemIndex=0 to beginWrite, causing every committed
keyed file to claim collection item zero before explicit binding. findSlot picks
the first matching slot, allowing unrelated art to appear for the first item.
Changed uploads to UINT8_MAX (unbound), and binding now removes older bindings
for the requested item before assigning the selected key.

Ctrl companion changes: first three at index zero use offsets 0,1,2 to match
boot priming; eviction resets legacy residency hints so missing files are not
skipped on future selection. Previous content-key unification remains required.

Build passed with RAM2 variables 255904 and free 268384, unchanged. Sixteen Ctrl
tests passed. Candidate is not flashed or physically verified. No new commits
or remote pushes. V2 restarted before these carousel edits to activate the
relative-seek fix; carousel Ctrl changes still require activation.
