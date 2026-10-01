param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8',
    [switch]$Capture,
    [switch]$PlayTest,
    [switch]$SkipBuild
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$ProjectPath = Join-Path $ProjectRoot 'prototype3.uproject'
$OutputDirectory = Join-Path $ProjectRoot 'Saved\InventoryVerification'
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
if (!$SkipBuild) {
    & (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') prototype3Editor Win64 Development "-Project=$ProjectPath" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) { throw "Unreal build failed: $LASTEXITCODE" }
}
$EditorArguments = @(
    $ProjectPath, '/Engine/Maps/Entry', '-unattended', '-nop4', '-nosplash', '-nosound',
    '-ExecCmds=Automation RunTests Prototype.Items+Prototype.Inventory',
    '-TestExit=Automation Test Queue Empty',
    "-ReportExportPath=$OutputDirectory\Tests",
    "-abslog=$OutputDirectory\tests.log"
)
if ($Capture) { $EditorArguments += @('-RenderOffscreen', '-InventoryCapture') }
else { $EditorArguments += '-NullRHI' }
if ($PlayTest) { $EditorArguments += '-InventoryPIE' }
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') @EditorArguments
if ($LASTEXITCODE -ne 0) { throw "Unreal tests failed: $LASTEXITCODE. See $OutputDirectory\tests.log" }
$Report = Get-Content -Raw (Join-Path $OutputDirectory 'Tests\index.json') | ConvertFrom-Json
$PassedCount = [int]$Report.succeeded + [int]$Report.succeededWithWarnings
if ($Report.failed -gt 0 -or $Report.notRun -gt 0 -or $PassedCount -lt 17) {
    throw 'The test report is incomplete or has failures. Inspect Saved\InventoryVerification.'
}
if ($Report.succeededWithWarnings -gt 0) { Write-Warning 'Tests passed with warnings; see the JSON report for details.' }
Write-Host "Inventory verification completed. Report: $OutputDirectory\Tests\index.json"
