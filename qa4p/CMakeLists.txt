# SPDX-License-Identifier: MIT-0
# SPDX-AI-Disclosure: ai-generated
# SPDX-AI-Model: claude-opus-5-5
# SPDX-AI-Provider: Anthropic
# ---------------------------------------------------------------------------
#  QA4P - QuickAssets 4 Pico
#
#  One library:
#    qa4p   the asset pack reader: files packed by tools/mkpack.py, found
#           by name, used in place in flash
#
#  It stands alone: it needs only the Pico SDK, nothing from qg4p or any
#  other QM4P library. Copy this folder into your project and, in your
#  project's CMakeLists.txt:
#      add_subdirectory(qa4p)
#      target_link_libraries(my_app pico_stdlib qa4p)
#  then #include "qa4p.h".
# ---------------------------------------------------------------------------
add_library(qa4p STATIC qa4p.c)
target_include_directories(qa4p PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(qa4p PUBLIC pico_stdlib)
