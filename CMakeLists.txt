cmake_minimum_required(VERSION 3.31)
project(pocketgb C)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options(-Wall -Wextra -Werror)
list(APPEND sources
    cpu.c
    lcd.c
    main.c
    mmu.c)
add_executable(pocketgb ${sources})
add_executable(disassembler disassembler.c)

find_package(SDL2 REQUIRED)
include_directories(pocketgb ${SDL2_INCLUDE_DIRS})
target_link_libraries(pocketgb ${SDL2_LIBRARIES})