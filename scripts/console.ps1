# Opens the connected board's USB-CDC console.
#
# USAGE:  scripts/console.ps1 [-Seconds 30] [-Out <file>] [-Until <regex>]
param(
        [int]$Seconds = 30,
        [string]$Out,
        [string]$Until,
        [switch]$Quiet
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'light-tools.ps1')

& (Join-Path $LightScripts 'light-console.ps1') -Seconds $Seconds -Out $Out -Until $Until -Quiet:$Quiet
