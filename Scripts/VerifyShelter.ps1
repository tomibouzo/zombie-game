param(
    [string]$EngineRoot = 'C:\UE_5.8',
    [switch]$SkipBuild,
    # Creates the placeholder piece assets and Lvl_BuildLab once. Existing files are never overwritten.
    [switch]$CreateFixtures,
    # Also runs the existing input, inventory and pause-menu checks that share files with build mode.
    [switch]$Regression,
    # Plays Lvl_BuildLab with the saved bindings and drives build mode through real input routing.
    [switch]$Play
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$ProjectPath = Join-Path $ProjectRoot 'prototype3.uproject'
$OutputDirectory = Join-Path $ProjectRoot 'Saved\ShelterVerification'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
if (!$SkipBuild) {
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') prototype3Editor Win64 Development "-Project=$ProjectPath" -WaitMutex -NoHotReloadFromIDE "-log=$OutputDirectory\Build.log"
    if ($LASTEXITCODE -ne 0) { throw "Shelter build failed: $LASTEXITCODE" }
}
$Editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$Stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
function Invoke-ShelterTests([string]$Tests, [string]$ReportName, [string[]]$ExtraArgs, [int]$MinimumCount) {
    $ReportPath = Join-Path $OutputDirectory "$ReportName-$Stamp"
    $EditorArgs = @($ProjectPath, '/Engine/Maps/Entry', '-unattended', '-nop4', '-nosplash', '-nosound',
        "-ExecCmds=Automation RunTests $Tests", '-TestExit=Automation Test Queue Empty',
        "-ReportExportPath=$ReportPath", "-abslog=$ReportPath.log") + $ExtraArgs
    & $Editor @EditorArgs
    if ($LASTEXITCODE -ne 0) { throw "Shelter checks failed: $LASTEXITCODE. See $ReportPath.log" }
    $Report = Get-Content -Raw (Join-Path $ReportPath 'index.json') | ConvertFrom-Json
    $Passed = [int]$Report.succeeded + [int]$Report.succeededWithWarnings
    if ($Report.failed -gt 0 -or $Report.notRun -gt 0 -or $Passed -lt $MinimumCount) {
        throw "Incomplete or failing report $ReportName : passed=$Passed failed=$($Report.failed) notRun=$($Report.notRun). See $ReportPath"
    }
    if ($Report.succeededWithWarnings -gt 0) { Write-Warning "$ReportName passed with warnings; inspect the report entries." }
    Write-Host "$ReportName : $Passed passed. Report: $ReportPath\index.json"
}
if ($CreateFixtures) { Invoke-ShelterTests 'Prototype.Fixtures.BuildLab' 'FixtureCreation' @('-NullRHI', '-ShelterCreateFixtures') 1 }
Invoke-ShelterTests 'Prototype.Shelter' 'Shelter' @('-NullRHI') 5
if ($Play) { Invoke-ShelterTests 'Prototype.ShelterPlay' 'Play' @('-RenderOffscreen', '-ShelterPIE') 1 }
if ($Regression) {
    Invoke-ShelterTests 'Prototype.Inventory.BackpackInputRouting+Prototype.Inventory.PauseMenu+Prototype.Inventory.HandPickupRules+Prototype.Inventory.HeldItemDrop+Prototype.Inventory.ItemActionContracts+Prototype.Movement' 'Regression' @('-NullRHI') 5
}
