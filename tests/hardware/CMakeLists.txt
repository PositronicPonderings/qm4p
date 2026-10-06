# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  Hardware test programs, one per development milestone.
#
#  Every program here builds as its own target, so a single build produces
#  all of them: build/tests/hardware/qg4p_test_m0.uf2 ... qg4p_test_m8.uf2. To try a
#  different one, flash a different .uf2; no CMake editing needed.
#
#  Wiring and board choice: test_setup.c (M2 onward) and docs/WIRING.md.
#  To flash and check every one in turn: sh tests/hardware/run_all.sh
# ---------------------------------------------------------------------------

# qg4p_test(name source... [ASSETS]): one test program.
function(qg4p_test name)
    cmake_parse_arguments(T "ASSETS" "" "" ${ARGN})
    add_executable(${name} ${T_UNPARSED_ARGUMENTS})
    target_link_libraries(${name} pico_stdlib qg4p)
    if (T_ASSETS)
        target_link_libraries(${name} qa4p)   # the asset pack library
    endif()
    pico_enable_stdio_usb(${name} 1)          # printf() goes to USB serial
    pico_enable_stdio_uart(${name} 0)
    # At start-up, stdio_init_all() waits until a terminal connects, but no
    # more than 2 seconds: with a terminal open the first lines aren't lost,
    # and with none the program still starts.
    target_compile_definitions(${name} PRIVATE PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=2000)
    pico_add_extra_outputs(${name})           # makes the .uf2
endfunction()

qg4p_test(qg4p_test_m0 test_m0.c)                                        # one ST7789
qg4p_test(qg4p_test_m1 test_m1.c)                                        # two screens
qg4p_test(qg4p_test_m2 test_m2.c test_setup.c)                           # shapes
qg4p_test(qg4p_test_m3 test_m3.c test_setup.c)                           # percentages
qg4p_test(qg4p_test_m4 test_m4.c test_setup.c)                           # fonts
qg4p_test(qg4p_test_m5 test_m5.c test_setup.c)                           # markup, scroll
qg4p_test(qg4p_test_m6 test_m6.c test_setup.c test_images.c)             # images
qg4p_test(qg4p_test_m7 test_m7.c test_setup.c ASSETS)                    # asset pack
qg4p_test(qg4p_test_m8 test_m8.c test_setup.c test_images.c)             # framebuffer
qg4p_test(qg4p_test_new_commands test_new_commands.c test_setup.c test_images.c)  # VIEW, GET/PUT, styles
