# Builds and flashes a screen-test target to a connected RP2 board over USB.
#
# USAGE:  scripts/flash.ps1 [-Target <name>] [-NoBuild]
param(
        [string]$Target,
        [string]$Preset,
        [switch]$NoBuild
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
. (Join-Path $PSScriptRoot 'light-tools.ps1')

if (-not $Target) { $Target = (& (Join-Path $PSScriptRoot 'project.config.ps1')).DefaultTarget }

& (Join-Path $LightScripts 'light-flash.ps1') -Target $Target -Preset $Preset -NoBuild:$NoBuild -ProjectRoot $root
