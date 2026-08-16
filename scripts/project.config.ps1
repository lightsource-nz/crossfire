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

        #   which OpenOCD config and SVD belong to each preset, for scripts/debug.ps1. Getting
        # this pairing wrong is not a clean failure: attaching an rp2040 configuration to an
        # rp2350 image misbehaves confusingly rather than erroring, which is why it is data here
        # rather than something the caller passes.
        #
        #   the host preset is deliberately absent. A HOST build is an ordinary executable
        # debugged with plain gdb -- there is no probe, no SVD and nothing for OpenOCD to do, and
        # light-debug.ps1 refuses with that in mind rather than half-working.
        Debug = @{
                'conf-crossfire-debug'       = @{
                        Config = 'openocd.cfg'
                        Svd    = '../pico-sdk/src/rp2040/hardware_regs/RP2040.svd'
                }
                'conf-crossfire-pico2-debug' = @{
                        Config = 'openocd-pico2.cfg'
                        Svd    = '../pico-sdk/src/rp2350/hardware_regs/RP2350.svd'
                }
                #   trace and release are rp2040 builds of the same firmware, so they debug
                # through the same config -- only the tree and the verbosity differ
                'conf-crossfire-trace'       = @{
                        Config = 'openocd.cfg'
                        Svd    = '../pico-sdk/src/rp2040/hardware_regs/RP2040.svd'
                }
                'conf-crossfire-release'     = @{
                        Config = 'openocd.cfg'
                        Svd    = '../pico-sdk/src/rp2040/hardware_regs/RP2040.svd'
                }
        }

        Test = @{
                Preset = 'conf-crossfire-host-debug'
                Ctest  = $true
        }

        #   HOST_OS, not the pico_hostmode this project's host preset uses: pico_hostmode is a
        # host build of the Pico SDK, and the coverage build is a plain Linux one. Only the
        # portable parts are measurable here, which for this project is a small share -- most of
        # crossfire is USB and transport code that only exists on target
        Coverage = @{
                Objects     = 'auto'
                IgnoreRegex = '(/lib/|/usr/|sanitizers/|_deps/|/freetype/|/jansson/|pico-sdk)'
                #   pico_hostmode, not HOST_OS: crossfire_forward_test includes hardware/gpio.h,
                # so it needs the Pico SDK's own host build and will not compile against a plain
                # Linux one.
                #   PICO_SDK_PATH and FONT_CRUSHER_PATH are NOT listed here. They used to be, as
                # absolute /mnt/c/... paths that only worked on one machine; light-coverage.ps1
                # now resolves both itself (user config, else the sibling checkout) and translates
                # them, so every project gets them without naming a path
                CMakeArgs   = @(
                        '-DLIGHT_SYSTEM=PICO_SDK', '-DLIGHT_PLATFORM=HOST',
                        '-DLIGHT_BOARD=pico_hostmode', '-DPICO_BOARD=none'
                )
        }
}
