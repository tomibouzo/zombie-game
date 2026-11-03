param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$SkipBuild,
    [switch]$CreateFixtures,
    [switch]$RebuildNavigation,
    [string]$DerivedDataCache = '',
    [switch]$Capture,
    [switch]$Regression
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
# Unreal uses a short existing junction when the checkout path exceeds common tool limits.
if (Test-Path 'C:\Users\valen\Documents\Codex\inventory-work\prototype3.uproject') {
    $CandidateRoot = Get-Item 'C:\Users\valen\Documents\Codex\inventory-work'
    if ($CandidateRoot.Target -contains $ProjectRoot) { $ProjectRoot = $CandidateRoot.FullName }
}
$ProjectPath = Join-Path $ProjectRoot 'prototype3.uproject'
$OutputDirectory = Join-Path $ProjectRoot 'Saved\ZombieVerification'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
if (!$SkipBuild) {
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') prototype3Editor Win64 Development "-Project=$ProjectPath" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) { throw "Unreal build failed: $LASTEXITCODE" }
}
$Editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
function Invoke-ZombieTests([string]$Tests, [string]$ReportName, [string[]]$ExtraArgs, [int]$MinimumCount) {
    $ReportPath = Join-Path $OutputDirectory $ReportName
    $EditorArgs = @($ProjectPath, '/Engine/Maps/Entry', '-unattended', '-nop4', '-nosplash', '-nosound',
        "-ExecCmds=Automation RunTests $Tests", '-TestExit=Automation Test Queue Empty',
        "-ReportExportPath=$ReportPath", "-abslog=$OutputDirectory\$ReportName.log") + $ExtraArgs
    if ($DerivedDataCache) { $EditorArgs += "-ddc=$DerivedDataCache" }
    & $Editor @EditorArgs
    if ($LASTEXITCODE -ne 0) { throw "Unreal verification failed: $LASTEXITCODE. See $ReportName.log" }
    $Report = Get-Content -Raw (Join-Path $ReportPath 'index.json') | ConvertFrom-Json
    $Count = [int]$Report.succeeded + [int]$Report.succeededWithWarnings
    if ($Report.failed -gt 0 -or $Report.notRun -gt 0 -or $Count -lt $MinimumCount) { throw "Incomplete or failed report: $ReportName ($Count passed; expected at least $MinimumCount)" }
    Write-Host "$ReportName : $Count passed"
}
if ($CreateFixtures) { Invoke-ZombieTests 'Prototype.Fixtures.ZombieLab' 'FixtureCreation' @('-NullRHI','-ZombieCreateFixtures') 1 }
if ($RebuildNavigation) { Invoke-ZombieTests 'Prototype.Fixtures.RebuildZombieNavigation' 'NavigationRebuild' @('-NullRHI','-ZombieRebuildNavigation') 1 }
$Rendering = if ($Capture) { @('-RenderOffscreen','-ZombieCapture') } else { @('-NullRHI') }
Invoke-ZombieTests 'Prototype.Zombies' 'Tests' $Rendering 5
if ($Regression) {
    Invoke-ZombieTests 'Prototype.Movement+Prototype.Items+Prototype.Inventory+Prototype.UI' 'Regression' @('-RenderOffscreen','-InventoryPIE') 41
}

