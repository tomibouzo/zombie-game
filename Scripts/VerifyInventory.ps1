param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
)

$ErrorActionPreference = 'Stop'
$projectDirectory = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectDirectory 'prototype3.uproject'
$buildTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$editorTool = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'

foreach ($requiredPath in @($projectFile, $buildTool, $editorTool)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required file is missing: $requiredPath"
    }
}

& $buildTool prototype3Editor Win64 Development "-Project=$projectFile" -WaitMutex
if ($LASTEXITCODE -ne 0) {
    throw 'Unreal compilation failed. Inventory tests were not run.'
}

$reportDirectory = Join-Path $projectDirectory ('Saved\InventoryTests\' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
& $editorTool $projectFile -unattended -nop4 -nosplash -NullRHI `
    '-ExecCmds=Automation RunTests Prototype.Inventory' `
    '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$reportDirectory" -log
if ($LASTEXITCODE -ne 0) {
    throw "Unreal automation failed. See $reportDirectory and Saved\Logs."
}

$reportFile = Join-Path $reportDirectory 'index.json'
if (-not (Test-Path -LiteralPath $reportFile)) {
    throw 'No automation report was produced; this is not a passing test run.'
}
$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
if ($report.failed -gt 0 -or $report.succeeded -lt 6 -or $report.notRun -gt 0 -or $report.inProcess -gt 0) {
    throw "Inventory test suite incomplete or failing. See $reportFile."
}
Write-Output "Inventory test suite passed. Report: $reportFile"
