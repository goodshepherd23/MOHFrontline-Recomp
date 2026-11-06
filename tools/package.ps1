# Builds MOHFrontline.exe, the time-zone shim and the release zip:
#   dist\MOHFrontline-Recomp-v<version>-windows-x64.zip
# Needs an llvm-mingw clang, a ModernGekko build with this repo's patches (moderngekko-run.exe, Sys\,
# dolrecomp.exe) and the MSVC v143 redistributable DLLs.
# Usage: powershell -File tools\package.ps1 -Clang <llvm-mingw>\bin\clang.exe -Build <ModernGekko>\build -Crt <...\Microsoft.VC143.CRT>
param(
    [string]$Clang = 'clang.exe',
    [Parameter(Mandatory = $true)][string]$Build,
    [Parameter(Mandatory = $true)][string]$Crt
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$version = (Get-Content -LiteralPath "$repo\kit\kit-version.txt" -Raw).Trim()
$stage = "$repo\dist\stage"
$zip = "$repo\dist\MOHFrontline-Recomp-v$version-windows-x64.zip"

if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path "$stage\runtime" | Out-Null
$windres = Join-Path (Split-Path -Parent $Clang) 'llvm-windres.exe'
& $windres "$repo\launcher\mohf_launcher.rc" -O coff -o "$repo\dist\mohf_launcher.res"
if ($LASTEXITCODE -ne 0) { throw 'launcher resource build failed' }
& $Clang -O2 -std=c11 -Wall -Wextra -municode -mwindows -static "-DKIT_VER=$version" `
    "$repo\launcher\mohf_launcher.c" "$repo\dist\mohf_launcher.res" -lole32 -lshell32 -luuid -o "$stage\MOHFrontline.exe"
if ($LASTEXITCODE -ne 0) { throw 'launcher build failed' }

# runtime\: the game-independent ModernGekko runtime, DolRecomp and the MSVC runtime they need.
Copy-Item -LiteralPath "$Build\moderngekko-run.exe", "$Build\dolrecomp.exe" -Destination "$stage\runtime"
Copy-Item -LiteralPath "$Build\Sys" -Destination "$stage\runtime\Sys" -Recurse
foreach ($dll in 'vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140.dll') {
    Copy-Item -LiteralPath "$Crt\$dll" -Destination "$stage\runtime"
}
Copy-Item -LiteralPath "$Crt\msvcp140_atomic_wait.dll" -Destination "$stage\runtime\msvcp140_atomic_wait_orig.dll"
& $Clang -O2 -shared -s "$repo\tzshim\tzshim.c" "$repo\tzshim\tzshim.def" -o "$stage\runtime\msvcp140_atomic_wait.dll"
if ($LASTEXITCODE -ne 0) { throw 'tzshim build failed' }

Copy-Item -LiteralPath "$repo\kit" -Destination "$stage\kit" -Recurse
foreach ($dir in 'patches', 'tzshim', 'launcher') { Copy-Item -LiteralPath "$repo\$dir" -Destination "$stage\$dir" -Recurse }
Copy-Item -LiteralPath "$repo\README.md", "$repo\LICENSE" -Destination $stage
Get-ChildItem -LiteralPath "$stage\kit" -Recurse -Directory -Filter __pycache__ | Remove-Item -Recurse -Force

if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip }
Compress-Archive -Path "$stage\*" -DestinationPath $zip
Get-Item -LiteralPath $zip | Select-Object Name, Length
