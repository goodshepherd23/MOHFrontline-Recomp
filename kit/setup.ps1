# Medal of Honor: Frontline (Recompiled) setup.
#
# Builds the game as native code on this PC: it reads YOUR disc, converts the game program
# (files\Moh2RelGC.elf) to a DOL, statically recompiles it with DolRecomp into C, and compiles that
# with a portable clang into the module the bundled ModernGekko runtime runs. Nothing from the game
# is shipped in this kit.
#
# Run by MOHFrontline.exe when needed; safe to re-run. -Root is the app folder (the one with kit\ and
# runtime\). The game files, the build and settings go in -Data, which MOHFrontline.exe sets to
# %LOCALAPPDATA%\MOHFrontline-Recomp so app updates leave them alone.
param(
    [Parameter(Mandatory = $true)][string]$Root,
    [string]$Data,
    [string]$Disc,
    [switch]$Rebuild
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'   # Invoke-WebRequest crawls with a progress bar on PowerShell 5.1
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$Root = (Resolve-Path -LiteralPath $Root).Path
if (-not $Data) { $Data = $Root }
New-Item -ItemType Directory -Force -Path $Data | Out-Null
$Data = (Resolve-Path -LiteralPath $Data).Path
$KitVersion = (Get-Content -LiteralPath "$Root\kit\kit-version.txt" -Raw).Trim()   # not version.txt: Quiver owns that file

# Downloads are pinned and must match their SHA-256.
$Pins = @{
    llvm   = @{ Url = 'https://github.com/mstorsjo/llvm-mingw/releases/download/20260616/llvm-mingw-20260616-ucrt-x86_64.zip'
                Sha = 'b9b68a4d276e16fa25802aaba458e4638f64b3884c290aaccdc2d87083b6ca35'; Dir = 'llvm-mingw' }
    python = @{ Url = 'https://www.python.org/ftp/python/3.14.6/python-3.14.6-embed-amd64.zip'
                Sha = 'df901e84a896ff1ee720ad03377e0c8d8c2244fda79808aeeaff6316df1cb75c'; Dir = 'python' }
}
# Medal of Honor: Frontline, USA (GMFE69). The recompiled program and its tuning are for this exact file.
$GameId  = 'GMFE69'
$ElfSha  = '8a3e0c1db8be94a7c1a320ca7586f59dd723d7f8768c2c711565af894fda589c'   # files\Moh2RelGC.elf
$DolSha  = 'a23783816c14a44c7432badf89ebac29440ae2c214c6cbebcc309c66ff921f52'   # the DOL made from it

$kit     = "$Root\kit"
$runtime = "$Root\runtime"
$tools   = "$Data\tools"
$dl      = "$tools\downloads"
$game    = "$Data\game"
$build   = "$Data\build"
$module  = "$Data\module"
$user    = "$Data\user"
New-Item -ItemType Directory -Force -Path $tools, $dl, $game, $module, $user | Out-Null
Start-Transcript -LiteralPath "$Data\setup.log" -Force | Out-Null

function Step([string]$m) { Write-Host ''; Write-Host "== $m" -ForegroundColor Cyan }
# Native tools write progress to stderr; under 'Stop', PowerShell 5.1 would make that fatal.
function Native([scriptblock]$b) { $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'; try { & $b } finally { $ErrorActionPreference = $old } }
function Fail([string]$m) { Write-Host ''; Write-Host "SETUP FAILED: $m" -ForegroundColor Red; Stop-Transcript | Out-Null; exit 1 }
function Sha([string]$f) { (Get-FileHash -LiteralPath $f -Algorithm SHA256).Hash.ToLower() }
# game\files may be a junction to YOUR extracted disc: unlink it first, so removing game\ can never
# reach through it and delete the disc's files.
function Remove-Game {
    $f = "$game\files"
    if ((Test-Path -LiteralPath $f) -and ((Get-Item -LiteralPath $f -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        [IO.Directory]::Delete($f, $false)
    }
    if (Test-Path -LiteralPath $game) { Remove-Item -LiteralPath $game -Recurse -Force }
}
function Read-Id([string]$f) { $b = New-Object byte[] 6; $s = [IO.File]::OpenRead($f); try { [void]$s.Read($b, 0, 6) } finally { $s.Dispose() }; [Text.Encoding]::ASCII.GetString($b) }

function Pick-File([string]$title, [string]$filter) {
    Add-Type -AssemblyName System.Windows.Forms
    $owner = New-Object System.Windows.Forms.Form -Property @{ TopMost = $true; ShowInTaskbar = $false }
    $dlg = New-Object System.Windows.Forms.OpenFileDialog -Property @{ Title = $title; Filter = $filter }
    try { if ($dlg.ShowDialog($owner) -eq 'OK') { return $dlg.FileName } } finally { $owner.Dispose() }
    return $null
}

# Downloads one pinned zip, checks it and unpacks it into tools\<Dir> (once; a marker records the
# checksum, so a changed pin replaces it). Returns the folder.
function Get-Pinned([string]$key) {
    $p = $Pins[$key]; $dest = "$tools\$($p.Dir)"; $marker = "$dest\.pinned"
    if ((Test-Path -LiteralPath $marker) -and (Get-Content -LiteralPath $marker -Raw).Trim() -eq $p.Sha) { return $dest }
    $zip = "$dl\$key.zip"
    Write-Host "Downloading $($p.Url.Split('/')[-1]) ..."
    Invoke-WebRequest $p.Url -OutFile $zip -UseBasicParsing
    if ((Sha $zip) -ne $p.Sha) { Remove-Item -LiteralPath $zip; Fail "The download of $($p.Url) failed its checksum." }
    if (Test-Path -LiteralPath $dest) { Remove-Item -LiteralPath $dest -Recurse -Force }
    $tmp = "$dl\$key.x"
    if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $tmp | Out-Null
    Native { & "$env:SystemRoot\System32\tar.exe" -xf $zip -C $tmp }
    if ($LASTEXITCODE -ne 0) { Fail "Could not unpack $zip." }
    $items = @(Get-ChildItem -LiteralPath $tmp -Force)
    if ($items.Count -eq 1 -and $items[0].PSIsContainer) { Move-Item -LiteralPath $items[0].FullName -Destination $dest }
    else { Move-Item -LiteralPath $tmp -Destination $dest }
    if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }
    Remove-Item -LiteralPath $zip
    Set-Content -LiteralPath $marker -Value $p.Sha
    return $dest
}

foreach ($f in "$runtime\moderngekko-run.exe", "$runtime\dolrecomp.exe", "$kit\build_module.py") {
    if (-not (Test-Path -LiteralPath $f)) { Fail "This kit is incomplete: $f is missing. Reinstall it." }
}

# ---------------------------------------------------------------- disc
Step 'Your Medal of Honor: Frontline disc'
$discFile = "$Data\disc.txt"
if (-not $Disc -and (Test-Path -LiteralPath $discFile)) {
    $saved = (Get-Content -LiteralPath $discFile -Raw).Trim()
    if ($saved -and (Test-Path -LiteralPath $saved)) { $Disc = $saved }
}
if (-not $Disc) {
    Write-Host 'Choose your Medal of Honor: Frontline (USA) disc image (.iso / .gcm),'
    Write-Host 'or, for a disc extracted with Dolphin, the files\Moh2RelGC.elf inside it.'
    Write-Host '(Other formats such as .rvz or .ciso: convert to ISO first in Dolphin, see README.)'
    $Disc = Pick-File 'Choose your Medal of Honor: Frontline disc' `
        'GameCube disc image (*.iso;*.gcm)|*.iso;*.gcm|Extracted disc program (Moh2RelGC.elf)|Moh2RelGC.elf'
    if (-not $Disc) { Fail 'No disc chosen.' }
}
$Disc = (Resolve-Path -LiteralPath $Disc).Path
if ((Get-Item -LiteralPath $Disc).PSIsContainer) {
    if (Test-Path -LiteralPath "$Disc\files\Moh2RelGC.elf") { $Disc = "$Disc\files\Moh2RelGC.elf" }
    else { Fail "$Disc is a folder without files\Moh2RelGC.elf." }
}
$ext = [IO.Path]::GetExtension($Disc).ToLower()
$stub = "$game\sys\main-stub.dol"
if ($ext -eq '.iso' -or $ext -eq '.gcm') {
    $id = Read-Id $Disc
    if ($id -ne $GameId) { Fail "This disc is $id, not Medal of Honor: Frontline USA ($GameId)." }
    $mark = "$game\.extracted-from"
    if (-not ((Test-Path -LiteralPath $mark) -and (Get-Content -LiteralPath $mark -Raw).Trim() -eq $Disc -and
              (Test-Path -LiteralPath "$game\files\Moh2RelGC.elf"))) {
        Write-Host "Extracting $Disc (about 1.4 GB) ..."
        Remove-Game
        Native { & "$runtime\dolrecomp.exe" extract $Disc $game 2>&1 | Select-Object -Last 3 | ForEach-Object { "$_" } }
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath "$game\files\Moh2RelGC.elf")) { Fail 'Could not extract that disc image.' }
        Move-Item -LiteralPath "$game\sys\main.dol" -Destination $stub -Force
        Set-Content -LiteralPath $mark -Value $Disc
    }
} elseif ([IO.Path]::GetFileName($Disc) -eq 'Moh2RelGC.elf') {
    $discRoot = Split-Path -Parent (Split-Path -Parent $Disc)
    if (-not (Test-Path -LiteralPath "$discRoot\sys\boot.bin")) { Fail "$discRoot has no sys\boot.bin; choose the Moh2RelGC.elf of a complete extracted disc." }
    $id = Read-Id "$discRoot\sys\boot.bin"
    if ($id -ne $GameId) { Fail "This disc is $id, not Medal of Honor: Frontline USA ($GameId)." }
    # The game reads its data straight from your extracted folder (a junction, no copy).
    Remove-Game
    New-Item -ItemType Directory -Force -Path "$game\sys" | Out-Null
    Copy-Item -Path "$discRoot\sys\*" -Destination "$game\sys" -Force
    Move-Item -LiteralPath "$game\sys\main.dol" -Destination $stub -Force
    Native { & cmd.exe /c mklink /J "$game\files" "$discRoot\files" | Out-Null }
    if (-not (Test-Path -LiteralPath "$game\files\Moh2RelGC.elf")) { Fail "Could not link $discRoot\files." }
} else {
    Fail "$Disc is not a .iso/.gcm image or Moh2RelGC.elf. Convert other formats to ISO in Dolphin first."
}
Set-Content -LiteralPath $discFile -Value $Disc
if ((Sha "$game\files\Moh2RelGC.elf") -ne $ElfSha) {
    Fail 'This disc''s game program (Moh2RelGC.elf) is not the revision this kit supports (the USA release).'
}
Write-Host "Disc: $Disc"

# ---------------------------------------------------------------- program
Step 'Preparing the game program'
$python = "$(Get-Pinned 'python')\python.exe"
$dol = "$game\sys\main.dol"
Native { & $python -I "$kit\elf2dol.py" "$game\files\Moh2RelGC.elf" $dol }
if ($LASTEXITCODE -ne 0 -or (Sha $dol) -ne $DolSha) { Fail 'Could not convert Moh2RelGC.elf to a DOL.' }

# Settings: the kit's defaults fill in only what is missing, so your own changes stay.
Native { & robocopy.exe "$kit\user" $user /E /XC /XN /XO /NFL /NDL /NJH /NJS /NP | Out-Null }
New-Item -ItemType Directory -Force -Path "$user\Cache\Shaders" | Out-Null   # the runtime never creates it; without it every start recompiles shaders

$dll = "$module\gGMFE69_recomp.dll"
$stamp = "$module\.kit-build"
if (-not $Rebuild -and (Test-Path -LiteralPath $dll) -and (Test-Path -LiteralPath $stamp) -and
    (Get-Content -LiteralPath $stamp -Raw).Trim() -eq $KitVersion) {
    Step 'The game is already built for this kit version.'
    Stop-Transcript | Out-Null; exit 0
}

# ---------------------------------------------------------------- recompile
Step 'Build tools (portable clang; about 600 MB, kept in tools\)'
$llvm = Get-Pinned 'llvm'
if (-not (Test-Path -LiteralPath "$llvm\bin\clang.exe")) { Fail "The build tools are incomplete: $llvm\bin\clang.exe is missing." }

Step 'Recompiling the game program to C (DolRecomp)'
if (Test-Path -LiteralPath $build) { Remove-Item -LiteralPath $build -Recurse -Force }
New-Item -ItemType Directory -Force -Path $build | Out-Null
Native { & robocopy.exe "$kit\module-src" $build /E /NFL /NDL /NJH /NJS /NP | Out-Null }
if ($LASTEXITCODE -ge 8) { Fail 'Could not copy the module sources.' }
$env:DOLRECOMP_SEMANTICS = 'fast'   # host floating point; see README
$jobs = [Environment]::ProcessorCount
Native { & "$runtime\dolrecomp.exe" "-j$jobs" --backend=c --cpu gekko --gamecube $dol $build 2>&1 | Select-String -Pattern 'chunks|direct|fast|error' | ForEach-Object { "$_" } }
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath "$build\generated\generated.h")) { Fail 'DolRecomp failed.' }
Copy-Item -LiteralPath $dol -Destination "$build\generated\main.dol" -Force

Step 'Compiling the game (about 10 to 25 minutes)'
$bm = @('-I', "$kit\build_module.py", '--root', $build, '--clang', "$llvm\bin", '--python', $python, '--out', $dll, "-j$jobs")
if (Test-Path -LiteralPath "$kit\profile\merged.profdata") { $bm += @('--profile-use', "$kit\profile\merged.profdata") }
if (Test-Path -LiteralPath $dll) { Remove-Item -LiteralPath $dll -Force }
Native { & $python @bm 2>&1 | ForEach-Object { "$_" } }
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $dll)) { Fail "The game did not compile. See $Data\setup.log." }
Remove-Item -LiteralPath "$build\obj" -Recurse -Force -ErrorAction SilentlyContinue   # ~1 GB of objects; the C stays for a rebuild

Set-Content -LiteralPath $stamp -Value $KitVersion
Write-Host ''
Write-Host 'Setup complete. Starting Medal of Honor: Frontline...' -ForegroundColor Green
Stop-Transcript | Out-Null
exit 0
