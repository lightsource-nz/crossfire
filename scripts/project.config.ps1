# Per-project defaults for crossfire.
#
# NOTE the firmware presets are split by chip: rp2040 into build/, rp2350 into build-pico2/.
# Every one of them used to resolve to ${sourceDir}/build, so only one chip's configuration
# could exist at a time and switching boards meant a -Force reconfigure every time. The presets
# carry the split now. build-link is the one tree left that no preset produces.
#
# ALSO: every crossfire tree currently has FONT_CRUSHER_PATH pointing into _deps at a copy of
# font-crusher fetched from GitHub, because crossfire's CMakeLists lacks the sibling-checkout
# resolution screen-test added. light-env.ps1 exports FONT_CRUSHER_PATH to correct that, but an
# already-configured tree keeps its cached value -- clean it to pick the change up.
@{
        Name = 'crossfire'

        Trees = @{
                #   SPLIT BY CHIP: the rp2040 presets own build/, the rp2350 ones own
                # build-pico2/. Both used to resolve to build/, so keeping an rp2040 and an
                # rp2350 configuration alive at once meant configuring one of them by hand --
                # which is exactly what had happened, and why this file used to describe
                # build-pico2 as a hand-made tree the scripts merely knew about. The two
                # configurations OF one chip still share a tree and still take turns under
                # light-configure.ps1's collision guard
                'conf-crossfire-debug'       = 'build'
                'conf-crossfire-link-debug'  = 'build'
                'conf-crossfire-pico2-debug' = 'build-pico2'
                'conf-crossfire-link-pico2-debug' = 'build-pico2'
                #   hub mode follows the same split: the rp2040 configuration shares build/
                # with the other rp2040 presets, the rp2350 one shares build-pico2
                'conf-crossfire-hub-debug'   = 'build'
                'conf-crossfire-hub-pico2-debug' = 'build-pico2'
                #   build-host, not build: this preset now overrides binaryDir so it stops
                # colliding with the two firmware presets. It is the only tree with tests in it
                'conf-crossfire-host-debug'  = 'build-host'
                'conf-crossfire-mini-stm32h7-debug' = 'build-mini-stm32h7'
                'conf-crossfire-link-stm32h7-debug' = 'build-mini-stm32h7'
                'conf-crossfire-trace'       = 'build-trace'
                'conf-crossfire-release'     = 'build-release'
        }

        Expect = @{
                'conf-crossfire-debug'       = @{ LIGHT_PLATFORM = 'TARGET'; LIGHT_BOARD = 'pico'; PICO_PLATFORM = 'rp2040' }
                'conf-crossfire-pico2-debug' = @{ LIGHT_PLATFORM = 'TARGET'; LIGHT_BOARD = 'pico2'; PICO_PLATFORM = 'rp2350-arm-s' }
                'conf-crossfire-host-debug'  = @{ LIGHT_PLATFORM = 'HOST'; LIGHT_BOARD = 'pico_hostmode' }
                'conf-crossfire-link-debug'  = @{ LIGHT_PLATFORM = 'TARGET'; LIGHT_BOARD = 'pico'; CROSSFIRE_ENABLE_SPI_LINK = 'ON' }
                'conf-crossfire-link-pico2-debug' = @{ LIGHT_PLATFORM = 'TARGET'; LIGHT_BOARD = 'pico2'; CROSSFIRE_ENABLE_SPI_LINK = 'ON' }
                #   SPI link is asserted OFF as well as hub ON: these presets share a tree with
                # conf-crossfire-debug, which turns the link on, and a stale cache carrying it
                # over is exactly the mix-up worth catching here
                'conf-crossfire-hub-debug'   = @{ LIGHT_PLATFORM = 'TARGET'; LIGHT_BOARD = 'pico'; CROSSFIRE_ENABLE_USB_HUB = 'ON'; CROSSFIRE_ENABLE_SPI_LINK = 'OFF' }
                'conf-crossfire-hub-pico2-debug' = @{ LIGHT_PLATFORM = 'TARGET'; LIGHT_BOARD = 'pico2'; CROSSFIRE_ENABLE_USB_HUB = 'ON'; CROSSFIRE_ENABLE_SPI_LINK = 'OFF' }
                'conf-crossfire-mini-stm32h7-debug' = @{ LIGHT_SYSTEM = 'CMSIS'; LIGHT_BOARD = 'mini_stm32h7' }
                'conf-crossfire-link-stm32h7-debug' = @{ LIGHT_SYSTEM = 'CMSIS'; LIGHT_BOARD = 'mini_stm32h7'; CROSSFIRE_ENABLE_SPI_LINK = 'ON' }
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
                'conf-crossfire-link-pico2-debug' = @{
                        Config = 'openocd-pico2.cfg'
                        Svd    = '../pico-sdk/src/rp2350/hardware_regs/RP2350.svd'
                }
                #   hub mode is the same two chips as the plain firmware presets, so it debugs
                # through the same probe configs -- only what is on the USB port differs
                'conf-crossfire-hub-debug'   = @{
                        Config = 'openocd.cfg'
                        Svd    = '../pico-sdk/src/rp2040/hardware_regs/RP2040.svd'
                }
                'conf-crossfire-hub-pico2-debug' = @{
                        Config = 'openocd-pico2.cfg'
                        Svd    = '../pico-sdk/src/rp2350/hardware_regs/RP2350.svd'
                }
                #   debugged over an ST-Link rather than CMSIS-DAP, and the only target here
                # with no UF2 path at all -- SWD is how an image reaches this board.
                #   no Svd: ST ships STM32H743.svd in their CMSIS pack rather than with the
                # toolchain, and none is vendored, so peripheral views are unavailable until
                # one is added. Everything else works without it.
                'conf-crossfire-mini-stm32h7-debug' = @{
                        Config = 'openocd-stm32h7.cfg'
                }
                'conf-crossfire-link-stm32h7-debug' = @{
                        Config = 'openocd-stm32h7.cfg'
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
