param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[a-z0-9][a-z0-9-]*$')]
    [string]$Name
)

$projectRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $projectRoot 'build\0.0.3\mkshft\firmware.hex'
$archiveRoot = Join-Path $projectRoot 'firmware-builds'

if (-not (Test-Path -LiteralPath $source)) {
    throw "Build firmware first; artifact not found: $source"
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$artifactName = "makeshift-$stamp-$Name.hex"
$destination = Join-Path $archiveRoot $artifactName
New-Item -ItemType Directory -Path $archiveRoot -Force | Out-Null
Copy-Item -LiteralPath $source -Destination $destination

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $destination).Hash.ToLowerInvariant()
"$hash  $artifactName" | Set-Content -LiteralPath "$destination.sha256" -Encoding ascii
Write-Output $destination
Write-Output "SHA256: $hash"
