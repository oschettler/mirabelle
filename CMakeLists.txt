cmake_minimum_required(VERSION 3.16)
project(pda C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

# --- Der portable Kern -------------------------------------------------------
# Eine Bibliothek je Schicht. Dass die Schichten getrennt übersetzt werden, ist
# der Mechanismus, der die Abhängigkeitsregel aus DESIGN.md Abschnitt 2 durchsetzt.

add_library(pda_core
    src/core/utf8.c
)
target_include_directories(pda_core PUBLIC src)

add_library(pda_gfx
    src/gfx/bitmap.c
    src/gfx/pbm.c
    src/gfx/pattern.c
    src/gfx/draw.c
    src/gfx/font.c
    src/gfx/text.c
)
target_include_directories(pda_gfx PUBLIC src)
target_link_libraries(pda_gfx PUBLIC pda_core)

# --- Plattformschicht --------------------------------------------------------
# Die Dateifunktionen sind für alles gleich, was ein POSIX-artiges Dateisystem
# hat, und liegen deshalb getrennt. Der ESP32 bekommt später eine eigene
# Umsetzung und bindet plat_files_posix.c einfach nicht ein.

add_library(pda_plat_headless
    src/plat/expand.c
    src/plat/plat_headless.c
    src/plat/plat_files_posix.c
)
target_link_libraries(pda_plat_headless PUBLIC pda_gfx)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL3 IMPORTED_TARGET sdl3)
endif()

if(TARGET PkgConfig::SDL3)
    add_library(pda_plat_sdl3
        src/plat/expand.c
        src/plat/plat_sdl3.c
        src/plat/plat_files_posix.c
    )
    target_link_libraries(pda_plat_sdl3 PUBLIC pda_gfx PRIVATE PkgConfig::SDL3)

    add_executable(pda src/main.c src/demo.c)
    target_link_libraries(pda PRIVATE pda_plat_sdl3 pda_fonts)
else()
    message(STATUS "SDL3 nicht gefunden - das Programm wird nicht gebaut, Tests laufen trotzdem")
endif()

# --- Werkzeuge ---------------------------------------------------------------
# fontc übersetzt .fnt-Zeichensätze nach C. Dieselbe Quelle wird zweimal
# gebaut: einmal als Programm für den Bau, einmal ohne main() als Bibliothek,
# damit der Test fontc_run() ohne Prozessstart aufrufen kann.

add_executable(fontc tools/fontc.c)
target_include_directories(fontc PRIVATE src tools)

add_library(pda_fontc tools/fontc.c)
target_compile_definitions(pda_fontc PRIVATE PDA_FONTC_NO_MAIN)
target_include_directories(pda_fontc PUBLIC src ${CMAKE_CURRENT_SOURCE_DIR})

# --- Zeichensätze ------------------------------------------------------------

set(PDA_FONT_SRC ${CMAKE_CURRENT_BINARY_DIR}/font_system12.c)
add_custom_command(
    OUTPUT  ${PDA_FONT_SRC}
    COMMAND fontc ${CMAKE_CURRENT_SOURCE_DIR}/data/fonts/system12.fnt
                  ${PDA_FONT_SRC} system12
    DEPENDS fontc ${CMAKE_CURRENT_SOURCE_DIR}/data/fonts/system12.fnt
    COMMENT "Zeichensatz system12 übersetzen"
    VERBATIM)

add_library(pda_fonts ${PDA_FONT_SRC})
target_link_libraries(pda_fonts PUBLIC pda_gfx)

# --- Tests -------------------------------------------------------------------

enable_testing()

add_library(pda_testsupport tests/support/golden.c)
target_link_libraries(pda_testsupport PUBLIC pda_gfx)
target_include_directories(pda_testsupport PUBLIC tests)
target_compile_definitions(pda_testsupport
    PRIVATE PDA_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/golden")

set(PDA_TESTS bitmap pbm golden draw utf8 fontc text expand plat)

foreach(name IN LISTS PDA_TESTS)
    add_executable(test_${name} tests/unit/test_${name}.c)
    target_link_libraries(test_${name} PRIVATE pda_testsupport)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()

target_link_libraries(test_fontc PRIVATE pda_fontc)

target_link_libraries(test_expand PRIVATE pda_plat_headless)
target_link_libraries(test_plat   PRIVATE pda_plat_headless)

target_link_libraries(test_text PRIVATE pda_fonts)
target_compile_definitions(test_text
    PRIVATE PDA_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
