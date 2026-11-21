cmake_minimum_required(VERSION 3.24)
project(Laba1_Sem_3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Предупреждения - только для нашего кода (не для GoogleTest)
function(laba1_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -pedantic)
    endif()
endfunction()

# Ядро: header-only шаблоны (Sequence из ЛР-2, деревья, производные структуры)
add_library(laba1_core INTERFACE)
target_include_directories(laba1_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Основная программа (консольный интерфейс)
add_executable(laba1 main.cpp)
target_include_directories(laba1 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(laba1 PRIVATE laba1_core)
laba1_warnings(laba1)

# Бенчмарк производительности
add_executable(laba1_benchmark include/benchmark/benchmark.cpp)
target_include_directories(laba1_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/benchmark)
target_link_libraries(laba1_benchmark PRIVATE laba1_core)
laba1_warnings(laba1_benchmark)

# Тесты на GoogleTest
option(LABA1_BUILD_TESTS "Build GoogleTest tests" ON)

if (LABA1_BUILD_TESTS)
    include(FetchContent)

    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)   # для MSVC
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()

    add_executable(laba1_tests
            tests/bs_tree_test.cpp         # этапы 1, 2, 4
            tests/traversal_test.cpp       # этап 3
    )
    target_link_libraries(laba1_tests PRIVATE laba1_core GTest::gtest_main)
    laba1_warnings(laba1_tests)

    include(GoogleTest)
    gtest_discover_tests(laba1_tests DISCOVERY_MODE PRE_TEST DISCOVERY_TIMEOUT 60)
endif()
