# Published firmware

Each pushed firmware checkpoint includes its exact `.hex` image and matching
`.hex.sha256` file here. Filenames are immutable and describe the build purpose.

Build, archive with `tools/archive_firmware.ps1 -Published`, verify the hash, and
commit the pair with the corresponding firmware source update.
