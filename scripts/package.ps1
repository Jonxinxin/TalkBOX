# Builds the distributable artifacts for a release:
#
#   dist/TalkBOX-portable-<version>.zip   portable folder (windeployqt'ed)
#   dist/TalkBOX-<version>-setup.exe      per-user installer (Inno Setup)
#
# Usage (from the repository root):
#   pwsh scripts/package.ps1 [-Configuration Release]

[CmdletBinding()]
param(
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# ---- read the version from CMakeLists.txt (single source of truth) ----
$cmakeLists = Get-Content -Raw (Join-Path $root 'CMakeLists.txt')
if ($cmakeLists -notmatch 'VERSION\s+(\d+)\.(\d+)\.(\d+)') {
    throw 'Could not read the project version from CMakeLists.txt'
}
$version = "$($Matches[1]).$($Matches[2]).$($Matches[3])"
Write-Host "TalkBOX version $version ($Configuration)"

# ---- locate Qt ----
$qtPrefix = $env:QT_ROOT
if (-not $qtPrefix) {
    if ($cmakeLists -match 'CMAKE_PREFIX_PATH:\s*"([^"]+)"') { $qtPrefix = $Matches[1] }
}
if (-not $qtPrefix) { $qtPrefix = 'C:/Qt/6.8.3/msvc2022_64' }
$windeployqt = Join-Path $qtPrefix 'bin/windeployqt.exe'
if (-not (Test-Path $windeployqt)) { throw "windeployqt not found at $windeployqt" }
Write-Host "Qt: $qtPrefix"

# ---- configure + build ----
cmake --preset windows-msvc
cmake --build "build/windows-msvc" --config $Configuration

$exe = "build/windows-msvc/$Configuration/TalkBOX.exe"
if (-not (Test-Path $exe)) { throw "Build output missing: $exe" }

# ---- portable folder ----
$portable = "dist/TalkBOX"
if (Test-Path $portable) { Remove-Item -Recurse -Force $portable }
New-Item -ItemType Directory -Force -Path $portable | Out-Null

# The QML sources are embedded in the exe, so only the Qt runtime has to be
# deployed. --qmldir is still passed so windeployqt picks up the QML modules
# (QtQuick Controls styles, dialogs, layouts) the app imports.
& $windeployqt --release --no-compiler-runtime --no-opengl-sw `
    --qmldir qml $exe --dir $portable
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed ($LASTEXITCODE)" }

Copy-Item $exe $portable -Force
Copy-Item 'LICENSES' $portable -Recurse -Force

# ---- trim what this app never loads ----
# Only the Basic style is used (QQuickStyle::setStyle("Basic")), only SQLite is
# needed, and qmltooling is a developer-only plugin set.
$trim = @(
    'qmltooling',
    'sqldrivers/qsqlmimer.dll',
    'sqldrivers/qsqlodbc.dll',
    'sqldrivers/qsqlpsql.dll',
    'translations/qtwebengine_locales'
)
foreach ($item in $trim) {
    $path = Join-Path $portable $item
    if (Test-Path $path) { Remove-Item -Recurse -Force $path }
}

# Unused Qt Quick Controls styles (Basic is the only one instantiated).
# Everything else in this directory (impl/, etc.) is shared plumbing and must
# stay.
$keep = @('Basic', 'impl')
$styles = Get-ChildItem (Join-Path $portable 'qml/QtQuick/Controls') -Directory -ErrorAction SilentlyContinue |
          Where-Object { $keep -notcontains $_.Name }
foreach ($style in $styles) { Remove-Item -Recurse -Force $style.FullName }

# Keep Chinese + English translations only.
Get-ChildItem (Join-Path $portable 'translations') -Filter '*.qm' -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -notmatch '_(zh_CN|en)\.qm$' } |
    Remove-Item -Force

# A short readme next to the exe for people who unzip instead of installing.
@"
TalkBOX $version
===============

双击 TalkBOX.exe 即可运行（免安装）。

- 数据目录：%LOCALAPPDATA%\TalkBOX
  （数据库 talkbox.db、附件 attachments、备份 backups、日志 logs）
- 卸载：直接删除本文件夹即可；如需彻底清除数据，再手动删除上面的数据目录。
- 升级：用新版覆盖本文件夹，数据会保留。

开源许可：见 LICENSES 目录。
"@ | Set-Content -Path (Join-Path $portable '使用说明.txt') -Encoding UTF8

$zip = "dist/TalkBOX-portable-$version.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path "$portable/*" -DestinationPath $zip -CompressionLevel Optimal
Write-Host "portable: $zip"

# ---- installer ----
$iscc = $env:ISCC_PATH
if (-not $iscc) {
    $candidates = @(
        'C:/Program Files (x86)/Inno Setup 6/ISCC.exe',
        'C:/Program Files/Inno Setup 6/ISCC.exe',
        "$env:LOCALAPPDATA/Programs/Inno Setup 6/ISCC.exe"
    )
    $iscc = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $iscc) {
    Write-Warning 'Inno Setup (ISCC.exe) not found - skipping the installer.'
    Write-Warning 'Install it with: winget install JRSoftware.InnoSetup'
} else {
    $env:TALKBOX_VERSION = $version
    & $iscc packaging/talkbox.iss
    if ($LASTEXITCODE -ne 0) { throw "ISCC failed ($LASTEXITCODE)" }
}

Get-ChildItem dist -File | Select-Object Name, @{ N = 'MB'; E = { [math]::Round($_.Length / 1MB, 1) } }
