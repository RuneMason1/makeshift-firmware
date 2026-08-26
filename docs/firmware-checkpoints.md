# Firmware checkpoints

Every flashable firmware checkpoint must contain enough information to rebuild
and audit the image without reconstructing its source from conversation history.

Required contents:

- `firmware.hex` and its SHA-256 hash
- `source.zip` containing the complete tracked firmware tree at the build commit
- `submodules/*.zip` containing each submodule at the revision used by that tree
- `build-metadata.json` recording the source commit and PlatformIO version
- a manifest describing retained features, known regressions, and hardware test
  results

Create a checkpoint with `scripts/archive-firmware-checkpoint.ps1`. Historical
HEX-only checkpoints should be backfilled when their exact source commit is
known. A legacy image that omits later features must be labeled as an emergency
rollback rather than a current firmware candidate.
