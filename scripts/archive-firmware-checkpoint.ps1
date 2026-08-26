param(
  [Parameter(Mandatory = $true)][string]$CheckpointPath,
  [Parameter(Mandatory = $true)][string]$Commit,
  [Parameter(Mandatory = $true)][string]$FirmwareHex
)

$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$checkpoint = [IO.Path]::GetFullPath($CheckpointPath)
$hex = (Resolve-Path $FirmwareHex).Path
$allowedRoot = [IO.Path]::GetFullPath(
  (Join-Path (Split-Path $repo -Parent) '.codex\checkpoints'))

if (!$checkpoint.StartsWith($allowedRoot, [StringComparison]::OrdinalIgnoreCase)) {
  throw "Checkpoint must be inside $allowedRoot"
}

New-Item -ItemType Directory -Path $checkpoint -Force | Out-Null
$sourceArchive = Join-Path $checkpoint 'source.zip'
& git -C $repo archive --format=zip -0 --output=$sourceArchive $Commit
if ($LASTEXITCODE -ne 0) { throw 'Unable to archive firmware source.' }

$submoduleRoot = Join-Path $checkpoint 'submodules'
New-Item -ItemType Directory -Path $submoduleRoot -Force | Out-Null
$submoduleLines = & git -C $repo config -f .gitmodules --get-regexp path
foreach ($line in $submoduleLines) {
  $path = ($line -split '\s+', 2)[1]
  $treeLine = & git -C $repo ls-tree $Commit -- $path
  if (!$treeLine) { continue }
  $revision = (($treeLine -split '\s+')[2])
  $archiveName = ($path -replace '[\\/]', '_') + '.zip'
  $submoduleArchive = Join-Path $submoduleRoot $archiveName
  & git -C (Join-Path $repo $path) archive --format=zip -0 `
    "--output=$submoduleArchive" $revision
  if ($LASTEXITCODE -ne 0) {
    throw "Unable to archive submodule $path at $revision"
  }
}

$archivedHex = Join-Path $checkpoint 'firmware.hex'
if (![IO.Path]::GetFullPath($hex).Equals(
    [IO.Path]::GetFullPath($archivedHex),
    [StringComparison]::OrdinalIgnoreCase)) {
  Copy-Item -LiteralPath $hex -Destination $archivedHex -Force
}
$metadata = [ordered]@{
  commit = (& git -C $repo rev-parse $Commit).Trim()
  firmwareSha256 = (Get-FileHash -Algorithm SHA256 $hex).Hash
  platformio = (& "$env:APPDATA\Python\Python312\Scripts\platformio.exe" --version).Trim()
  archivedAt = (Get-Date).ToString('o')
}
[IO.File]::WriteAllText(
  (Join-Path $checkpoint 'build-metadata.json'),
  ($metadata | ConvertTo-Json) + [Environment]::NewLine)

Write-Output ($metadata | ConvertTo-Json)
