cmake_minimum_required(VERSION 4.1)
project(lispy C)

set(CMAKE_C_STANDARD 11)

add_executable(lispy
        deps/mpc.c
        main.c
)

target_link_libraries(lispy edit)
target_link_libraries(lispy m)

target_compile_options(lispy PUBLIC -fsanitize=address)

target_link_options(lispy PUBLIC -fsanitize=address)

include_directories(deps)