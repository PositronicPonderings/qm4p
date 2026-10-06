# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# ---------------------------------------------------------------------------
#  QM4P - QuickMedia 4 Pico: the development project for every library,
#  example and test program.
#
#  Each library folder is self-contained, needing only the Pico SDK:
#    qg4p/   QuickGraphics 4 Pico   (CMake target qg4p)
#    qa4p/   QuickAssets 4 Pico     (CMake target qa4p)
#  To USE them in your own project, copy only the folders you use; see the
#  CMakeLists.txt inside each. This file builds the whole repository.
#
#  Opening this folder with the Raspberry Pi Pico VS Code extension: the
#  extension may offer to add its own settings block at the top of this
#  file. That's fine; keep it above the lines below, but keep the four
#  SPDX lines first: the AI-disclosure check (tests/host) reads only the
#  first few lines of each file.
# ---------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_BOARD pico2 CACHE STRING "Board type")

include(pico_sdk_import.cmake)
project(qm4p C CXX ASM)
pico_sdk_init()

add_subdirectory(qg4p)             # the graphics library: qg4p
add_subdirectory(qa4p)             # the asset pack library: qa4p
add_subdirectory(examples)         # examples: qg4p_hello, ...
add_subdirectory(tests/hardware)   # hardware test programs qg4p_m0 ... qg4p_m8
