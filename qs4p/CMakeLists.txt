# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  QS4P - QuickSound 4 Pico (in development)
#
#  One library:
#    qs4p   sound: beeps and tones, played through PWM and an amplifier
#
#  It stands alone: it needs only the Pico SDK, nothing from qg4p, qa4p or
#  any other QM4P library. Copy this folder into your project and, in your
#  project's CMakeLists.txt:
#      add_subdirectory(qs4p)
#      target_link_libraries(my_app pico_stdlib qs4p)
#  then #include "qs4p.h".
#
#  Each part is its own file, and the linker keeps only the files a
#  program calls into: no qs_init_pwm(), no PWM backend; no qs_sound() or
#  qs_beep(), no tone code.
# ---------------------------------------------------------------------------
add_library(qs4p STATIC
    qs_engine.c     # what's playing, volume, the amplifier
    qs_pwm.c        # the PWM backend: DMA, pacing timer, interrupt
    qs_tone.c       # tones: BEEP and SOUND
)
target_include_directories(qs4p PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(qs4p PUBLIC
    pico_stdlib
    hardware_pwm
    hardware_dma
    hardware_irq
)
