# Per-project defaults for crossfire.
#
# NOTE all six committed presets resolve to ${sourceDir}/build, which is why three of the four
# trees on disk (build-host, build-link, build-pico2) were configured by hand -- there was no
# other way to keep an rp2040 and an rp2350 configuration alive at the same time. They are
# recorded here so the scripts can find them.
#
# ALSO: every crossfire tree currently has FONT_CRUSHER_PATH pointing into _deps at a copy of
# font-crusher fetched from GitHub, because crossfire's CMakeLists lacks the sibling-checkout
# resolution screen-test added. light-env.ps1 exports FONT_CRUSHER_PATH to correct that, but an
# already-configured tree keeps its cached value -- clean it to pick the change up.
@{
        Name = 'crossfire'

        Trees = @{
                'conf-crossfire-debug'       = 'build'
                'conf-crossfire-pico2-debug' = 'build'
                'conf-crossfire-host-debug'  = 'build'
                'conf-crossfire-trace'       = 'build-trace'
                'conf-crossfire-release'     = 'build-release'
        }

        Expect = @{
                'conf-crossfire-debug'       = @{ LIGHT_PLATFORM = 'TARGET'; LIGHT_BOARD = 'pico'; PICO_PLATFORM = 'rp2040' }
                'conf-crossfire-pico2-debug' = @{ LIGHT_PLATFORM = 'TARGET'; LIGHT_BOARD = 'pico2'; PICO_PLATFORM = 'rp2350-arm-s' }
                'conf-crossfire-host-debug'  = @{ LIGHT_PLATFORM = 'HOST'; LIGHT_BOARD = 'pico_hostmode' }
                'conf-crossfire-trace'       = @{ LIGHT_PLATFORM = 'TARGET'; LIGHT_BOARD = 'pico'; LIGHT_RUN_MODE = 'TRACE' }
                'conf-crossfire-release'     = @{ LIGHT_PLATFORM = 'TARGET'; LIGHT_BOARD = 'pico'; LIGHT_RUN_MODE = 'PRODUCTION' }
        }

        Targets = @{
                'crossfire_main'         = @{ Preset = 'conf-crossfire-pico2-debug'; Flash = 'uf2' }
                'crossfire_forward_test' = @{ Preset = 'conf-crossfire-host-debug' }
        }

        DefaultTarget = 'crossfire_main'

        Test = @{
                Preset = 'conf-crossfire-host-debug'
                Ctest  = $true
        }
}
