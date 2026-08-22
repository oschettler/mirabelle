cmake_minimum_required(VERSION 3.16)
project(pda C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -Wpedantic)

# --- Der portable Kern -------------------------------------------------------
# Eine Bibliothek je Schicht. Dass die Schichten getrennt übersetzt werden, ist
# der Mechanismus, der die Abhängigkeitsregel aus DESIGN.md Abschnitt 2 durchsetzt.

add_library(pda_gfx
    src/gfx/bitmap.c
    src/gfx/pbm.c
    src/gfx/pattern.c
    src/gfx/draw.c
)
target_include_directories(pda_gfx PUBLIC src)

# --- Tests -------------------------------------------------------------------

enable_testing()

add_library(pda_testsupport tests/support/golden.c)
target_link_libraries(pda_testsupport PUBLIC pda_gfx)
target_include_directories(pda_testsupport PUBLIC tests)
target_compile_definitions(pda_testsupport
    PRIVATE PDA_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/golden")

set(PDA_TESTS bitmap pbm golden draw)

foreach(name IN LISTS PDA_TESTS)
    add_executable(test_${name} tests/unit/test_${name}.c)
    target_link_libraries(test_${name} PRIVATE pda_testsupport)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()
