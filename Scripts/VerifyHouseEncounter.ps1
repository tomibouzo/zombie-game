param(
    [string]$EngineRoot = 'C:\UE_5.8',
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$ProjectPath = Join-Path $ProjectRoot 'prototype3.uproject'
$OutputDirectory = Join-Path $ProjectRoot 'Saved\HouseEncounterVerification'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
if (!$SkipBuild) {
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') prototype3Editor Win64 Development "-Project=$ProjectPath" -WaitMutex -NoHotReloadFromIDE "-log=$OutputDirectory\Build.log"
    if ($LASTEXITCODE -ne 0) { throw "Encounter build failed: $LASTEXITCODE" }
}
$Tests = @(
    'Prototype.Encounter',
    'Prototype.House.LayoutSeeds',
    'Prototype.House.GeometryAndCollision',
    'Prototype.Items',
    'Prototype.Inventory.FloorTransfers',
    'Prototype.Inventory.HeldItemDrop',
    'Prototype.Inventory.HandPickupRules',
    'Prototype.Inventory.ItemActionContracts',
    'Prototype.Inventory.BackpackInputRouting',
    'Prototype.Inventory.PauseMenu'
) -join '+'
$RunDirectory = Join-Path $OutputDirectory ('Checks-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$EditorArguments = @(
    $ProjectPath, '/Engine/Maps/Entry', '-unattended', '-nop4', '-nosplash', '-nosound', '-NullRHI',
    "-ExecCmds=Automation RunTests $Tests", '-TestExit=Automation Test Queue Empty',
    "-ReportExportPath=$RunDirectory", "-abslog=$RunDirectory.log"
)
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') @EditorArguments
if ($LASTEXITCODE -ne 0) { throw "Encounter checks failed: $LASTEXITCODE. See $RunDirectory.log" }
$Report = Get-Content -Raw (Join-Path $RunDirectory 'index.json') | ConvertFrom-Json
$Passed = [int]$Report.succeeded + [int]$Report.succeededWithWarnings
if ($Report.failed -gt 0 -or $Report.notRun -gt 0 -or $Passed -lt 19) {
    throw "Incomplete or failing encounter report: passed=$Passed failed=$($Report.failed) notRun=$($Report.notRun). See $RunDirectory"
}
if ($Report.succeededWithWarnings -gt 0) { Write-Warning 'Checks passed with warnings; inspect the report entries.' }
Write-Host "Passed $Passed checks. Report: $RunDirectory\index.json"
