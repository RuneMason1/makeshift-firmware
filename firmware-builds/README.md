# Firmware rollback builds

These `.hex` files are verified PlatformIO build artifacts retained as rollback
points. Each image has a matching `.sha256` file. Generate a new archive after a
successful build with:

```powershell
.\tools\archive_firmware.ps1 -Name descriptive-build-name
```

Do not replace an existing artifact. Use a new descriptive name for each build.
