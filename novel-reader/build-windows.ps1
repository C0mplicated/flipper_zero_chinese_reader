$ErrorActionPreference = 'Stop'
$ProjectRoot = $PSScriptRoot
$SdkRoot = Join-Path $ProjectRoot 'momentum-sdk'
$Commit = 'd3f89dfe2ef6b01839201598e9be1590cba80322'
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'Install Git for Windows, reopen PowerShell, then run this script again.'
}
if (-not (Test-Path $SdkRoot)) {
    git clone https://github.com/Next-Flip/Momentum-Firmware.git $SdkRoot
    if ($LASTEXITCODE -ne 0) { throw 'Clone failed.' }
    git -C $SdkRoot checkout --detach $Commit
    if ($LASTEXITCODE -ne 0) { throw 'Pinned commit checkout failed.' }
}
$Actual = git -C $SdkRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $Actual -ne $Commit) { throw 'SDK version differs. Use a fresh project folder; existing checkout was not modified.' }
$Dirty = git -C $SdkRoot status --porcelain --untracked-files=no
if ($Dirty) { throw 'SDK has tracked edits; refusing to build against modified firmware.' }
git -C $SdkRoot submodule update --init --recursive
if ($LASTEXITCODE -ne 0) { throw 'Submodule download failed.' }
$AppTarget = Join-Path $SdkRoot 'applications_user/novel_reader'
New-Item -ItemType Directory -Force $AppTarget | Out-Null
Copy-Item (Join-Path $ProjectRoot 'src/*') $AppTarget -Recurse -Force
Push-Location $SdkRoot
try {
    & .\fbt.cmd fap_novel_reader
    if ($LASTEXITCODE -ne 0) { throw 'Build failed; send the error text for diagnosis.' }
    $Fap = Get-ChildItem (Join-Path $SdkRoot 'build') -Filter novel_reader.fap -Recurse |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $Fap) { throw 'Build returned no FAP.' }
    Copy-Item $Fap.FullName (Join-Path $ProjectRoot 'sdcard/apps/Tools/novel_reader.fap') -Force
    Write-Host 'Built: sdcard/apps/Tools/novel_reader.fap. No firmware was flashed.'
} finally { Pop-Location }
