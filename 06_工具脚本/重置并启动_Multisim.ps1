param(
    [switch]$Launch
)

$ErrorActionPreference = "Stop"

$multisimPath = "C:\Program Files (x86)\National Instruments\Circuit Design Suite 14.3\multisim.exe"
$userDbDir = "C:\Users\powen\AppData\Roaming\National Instruments\Circuit Design Suite\14.3\database"
$sharedDbDir = "C:\ProgramData\National Instruments\Circuit Design Suite\14.3\database"
$backupRoot = Join-Path $PSScriptRoot "multisim_lock_backups"
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"

function Move-LockFiles {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourceDir,
        [Parameter(Mandatory = $true)]
        [string]$Tag
    )

    if (-not (Test-Path -LiteralPath $SourceDir)) {
        Write-Host "[skip] not found: $SourceDir"
        return
    }

    $files = Get-ChildItem -LiteralPath $SourceDir -Filter "*.ldb" -ErrorAction SilentlyContinue
    if (-not $files) {
        Write-Host "[ok] no lock files in $SourceDir"
        return
    }

    $targetDir = Join-Path $backupRoot "${timestamp}_$Tag"
    New-Item -ItemType Directory -Path $targetDir -Force | Out-Null

    foreach ($file in $files) {
        $target = Join-Path $targetDir $file.Name
        Move-Item -LiteralPath $file.FullName -Destination $target -Force
        Write-Host "[move] $($file.FullName) -> $target"
    }
}

New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null

$procs = Get-Process multisim, ultiboard -ErrorAction SilentlyContinue
if ($procs) {
    $procs | Stop-Process -Force
    Start-Sleep -Seconds 2
    Write-Host "[ok] stopped existing Multisim/Ultiboard processes"
} else {
    Write-Host "[ok] no running Multisim/Ultiboard process"
}

Move-LockFiles -SourceDir $sharedDbDir -Tag "shared"
Move-LockFiles -SourceDir $userDbDir -Tag "user"

Write-Host "[info] reset complete"

if ($Launch) {
    if (-not (Test-Path -LiteralPath $multisimPath)) {
        throw "Multisim not found: $multisimPath"
    }

    Start-Process -FilePath $multisimPath
    Write-Host "[info] Multisim launch requested"
}
