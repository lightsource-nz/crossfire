# Locates the shared script layer, setting $LightScripts for the wrappers in this directory.
#
# WHY THIS EXISTS: the wrappers reach the framework through LIGHT_PATH, which is the convention
# CMake already uses (screen-test/CMakeLists.txt reads it from the environment before anything
# else). But a fresh shell may not have it set, and failing with "cannot find light-build.ps1"
# would send you looking in the wrong place -- so fall back to the sibling checkout, which is
# the same default CMakeLists.txt uses, and say clearly when neither works.
$ErrorActionPreference = 'Stop'

$candidate = if ($env:LIGHT_PATH) {
        $env:LIGHT_PATH
} else {
        # forward slashes: PowerShell accepts them on Windows, while a backslash on Linux is an
        # ordinary filename character, so '..\..\x' there names one file that does not exist
        Join-Path $PSScriptRoot '../../light_framework_mk3'
}

if (-not (Test-Path (Join-Path $candidate 'scripts/light-build.ps1'))) {
        throw "cannot find the light framework scripts. Set LIGHT_PATH to the light_framework_mk3 checkout (tried '$candidate')."
}

$LightScripts = (Resolve-Path (Join-Path $candidate 'scripts')).Path
