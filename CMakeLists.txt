cmake_minimum_required(VERSION 3.16)
project(distiller_c11 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

# --- library: all modules as one static lib (each module still
#     self-contained; this is just the link unit) ---
add_library(distiller_c11 STATIC
  src/dither/dither.c
  src/pack/pack.c
  src/image/image.c
  src/compose/compose.c
  src/eink/eink.c
  src/hw/sam/sam.c
  src/hw/einkdrv/einkdrv.c
  src/hw/audio/audio.c
  src/ui/registry/registry.c
  src/ui/gui/gui.c
  src/ai/ai.c
)
target_include_directories(distiller_c11 PUBLIC src)

# --- tests ---
enable_testing()
add_executable(test_dither tests/test_dither.c)
target_link_libraries(test_dither distiller_c11)
add_test(NAME dither COMMAND test_dither)

add_executable(test_pack tests/test_pack.c)
target_link_libraries(test_pack distiller_c11)
add_test(NAME pack COMMAND test_pack)

add_executable(test_image tests/test_image.c)
target_link_libraries(test_image distiller_c11)
add_test(NAME image COMMAND test_image)

add_executable(test_eink tests/test_eink.c)
target_link_libraries(test_eink distiller_c11)
add_test(NAME eink COMMAND test_eink)

add_executable(test_compose tests/test_compose.c)
target_link_libraries(test_compose distiller_c11)
add_test(NAME compose COMMAND test_compose)

# --- hw module tests (need pthread for sam) ---
add_executable(test_sam tests/test_sam.c)
target_link_libraries(test_sam distiller_c11 pthread)
add_test(NAME sam COMMAND test_sam)

add_executable(test_einkdrv tests/test_einkdrv.c)
target_link_libraries(test_einkdrv distiller_c11)
add_test(NAME einkdrv COMMAND test_einkdrv)

add_executable(test_audio tests/test_audio.c)
target_link_libraries(test_audio distiller_c11)
add_test(NAME audio COMMAND test_audio)

add_executable(test_registry tests/test_registry.c)
target_link_libraries(test_registry distiller_c11)
add_test(NAME registry COMMAND test_registry)

add_executable(test_ai tests/test_ai.c)
target_link_libraries(test_ai distiller_c11)
add_test(NAME ai COMMAND test_ai)

add_executable(test_gui tests/test_gui.c)
target_link_libraries(test_gui distiller_c11)
add_test(NAME gui COMMAND test_gui)

# --- tools ---
add_executable(bench_eink tools/bench_eink.c)
target_link_libraries(bench_eink distiller_c11)

add_executable(bench_both tools/bench_both.c)
target_link_libraries(bench_both distiller_c11)
