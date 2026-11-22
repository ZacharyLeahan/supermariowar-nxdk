param(
    [string]$Destination = ""
)

$ErrorActionPreference = "Stop"
$kit = Split-Path -Parent $PSScriptRoot
$lock = Get-Content -LiteralPath (Join-Path $kit "upstream-lock.json") -Raw | ConvertFrom-Json
$tool = $lock.host_toolchains.windows_msys2
$cacheRoot = Join-Path $env:LOCALAPPDATA "SuperMarioWarXbox"
$toolRoot = Join-Path $cacheRoot $tool.release
$archive = Join-Path $cacheRoot "msys64-$($tool.release).7z"
$bash = Join-Path $toolRoot "msys64\usr\bin\bash.exe"

if (-not $Destination) {
    $desktop = [Environment]::GetFolderPath("Desktop")
    if (-not $desktop) { $desktop = (Get-Location).Path }
    $Destination = Join-Path $desktop "SuperMarioWar-Xbox"
}
$Destination = [IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $Destination) {
    throw "Output already exists. Move or remove it first: $Destination"
}

New-Item -ItemType Directory -Force -Path $cacheRoot | Out-Null
if (-not (Test-Path -LiteralPath $bash)) {
    if (-not (Test-Path -LiteralPath $archive)) {
        Write-Host "Downloading the pinned native Xbox build toolchain (about 489 MiB)..."
        & curl.exe -L --fail --output $archive $tool.url
        if ($LASTEXITCODE -ne 0) { throw "Toolchain download failed." }
    }
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash.ToLowerInvariant()
    if ($actual -ne $tool.sha256) {
        throw "Toolchain checksum mismatch. Delete $archive and try again."
    }
    Write-Host "Extracting the verified native Windows toolchain..."
    New-Item -ItemType Directory -Force -Path $toolRoot | Out-Null
    & tar.exe -xf $archive -C $toolRoot
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $bash)) {
        throw "Toolchain extraction failed."
    }
}

$env:MSYSTEM = "MINGW64"
$env:CHERE_INVOKING = "1"
Write-Host "Ensuring the toolchain's Python runner and comparison tools are installed..."
& $bash -lc "pacman -Sy --noconfirm --needed python diffutils"
if ($LASTEXITCODE -ne 0) { throw "Python installation in the native toolchain failed." }

$env:SMW_KIT_WINDOWS = $kit
$env:SMW_DEST_WINDOWS = $Destination
$jobs = [Math]::Max(1, [Math]::Min(16, [Environment]::ProcessorCount))
Write-Host "Building the pinned source and data without WSL..."
& $bash -lc 'kit=$(cygpath -u "$SMW_KIT_WINDOWS"); dest=$(cygpath -u "$SMW_DEST_WINDOWS"); cd "$kit"; exec python3 build-xbox.py --destination "$dest" --jobs '"$jobs"
if ($LASTEXITCODE -ne 0) { throw "Xbox build failed." }
