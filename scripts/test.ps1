# Runs this project's host test suite and smoke checks.
#
# USAGE:  scripts/test.ps1 [-Preset <name>] [-NoBuild]
param(
        [string]$Preset,
        [switch]$NoBuild
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
. (Join-Path $PSScriptRoot 'light-tools.ps1')

& (Join-Path $LightScripts 'light-test.ps1') -Preset $Preset -NoBuild:$NoBuild -ProjectRoot $root
