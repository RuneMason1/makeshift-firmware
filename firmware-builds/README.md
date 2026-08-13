# Firmware rollback builds

These `.hex` files are verified PlatformIO build artifacts retained as rollback
points. Each image has a matching `.sha256` file. Generate a new archive after a
successful build with:

```powershell
.\tools\archive_firmware.ps1 -Name descriptive-build-name
```

Do not replace an existing artifact. Use a new descriptive name for each build.

## Published firmware

Every pushed firmware checkpoint must include the exact corresponding HEX and
SHA-256 under `firmware-builds/published`. Create both after the final verified
build with:

```powershell
.\tools\archive_firmware.ps1 -Name descriptive-build-name -Published
```

Commit the generated pair in the same commit as the source, or in an immediately
adjacent artifact commit that names the source commit. Local experiments should
continue using the default command without `-Published` and remain ignored.
