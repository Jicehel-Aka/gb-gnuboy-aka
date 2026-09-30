# ============================================================================
# Frontend PC (SDL2) : compile gb_gnuboy_pc avec le même code d'émulation que la console.
#
# Linux : cmake -S pc -B build-pc && cmake --build build-pc   (libsdl2-dev requis)   Windows : MSYS2/MinGW, voir .github/workflows/build-pc.yml
# Projet : gb-gnuboy-aka (Gamebuino AKA). Auteur : Jicehel. Voir README.md.
# ============================================================================
cmake_minimum_required(VERSION 3.16)
project(gb_gnuboy_pc C)

set(CMAKE_C_STANDARD 11)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(GBRT_DIR ${CMAKE_CURRENT_LIST_DIR}/../components/gbrt_aka)
set(GNUBOY_DIR ${CMAKE_CURRENT_LIST_DIR}/../components/gnuboy)
set(GNUBOY_SRCS ${GNUBOY_DIR}/cpu.c ${GNUBOY_DIR}/gnuboy.c ${GNUBOY_DIR}/hw.c ${GNUBOY_DIR}/lcd.c ${GNUBOY_DIR}/sound.c)

find_package(SDL2 REQUIRED)

add_executable(gb_gnuboy_pc
  main_pc.c
  pc_ui.c
  ${GBRT_DIR}/src/gbrt_aka.c
  ${GBRT_DIR}/src/gbrt_browser.c
  ${GBRT_DIR}/src/gbrt_zip.c
  ${GNUBOY_SRCS})

target_include_directories(gb_gnuboy_pc PRIVATE
  ${GBRT_DIR}/include ${GNUBOY_DIR})

if(TARGET SDL2::SDL2)
  target_link_libraries(gb_gnuboy_pc PRIVATE SDL2::SDL2)
else()
  target_include_directories(gb_gnuboy_pc PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(gb_gnuboy_pc PRIVATE ${SDL2_LIBRARIES})
endif()

if(NOT MSVC)
  target_compile_options(gb_gnuboy_pc PRIVATE -O2 -Wno-unused-function)
  target_link_libraries(gb_gnuboy_pc PRIVATE m)
endif()
if(MINGW)
  target_link_options(gb_gnuboy_pc PRIVATE -static-libgcc)
endif()
