# Starts an OpenOCD server, and optionally gdb, against a built crossfire target.
#
# USAGE:  scripts/debug.ps1 [-Target <name>] [-ServerOnly] [-Attach] [-NoBuild]
#
#   -ServerOnly leaves OpenOCD in the foreground for a debugger to attach to on localhost:3333,
# which is what VS Code's launch configurations want. Without it, gdb is started too.
#   -Attach leaves whatever is already on the board alone; the default loads the ELF just built.
#
#   NOTE the default target builds for RP2350 (conf-crossfire-pico2-debug) and debugs through
# openocd-pico2.cfg. The rp2040 presets share ${sourceDir}/build with it, so if the tree was last
# configured for a different one, build first or pass -Preset -- a debug session against an image
# built for the other chip misbehaves rather than failing cleanly.
param(
        [string]$Target,
        [string]$Preset,
        [switch]$ServerOnly,
        [switch]$Attach,
        [switch]$NoBuild
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
. (Join-Path $PSScriptRoot 'light-tools.ps1')

if (-not $Target) { $Target = (& (Join-Path $PSScriptRoot 'project.config.ps1')).DefaultTarget }

& (Join-Path $LightScripts 'light-debug.ps1') -Target $Target -Preset $Preset `
        -ServerOnly:$ServerOnly -Attach:$Attach -NoBuild:$NoBuild -ProjectRoot $root
