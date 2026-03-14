cmake_minimum_required(VERSION 4.1)
project(lispy C)

set(CMAKE_C_STANDARD 11)

add_executable(lispy
        main.c
        lval.c
        parsing.c
        eval.c
        deps/mpc.c
        lenv.c
)

target_link_libraries(lispy edit)
target_link_libraries(lispy m)

target_compile_options(lispy PUBLIC -fsanitize=address)

target_link_options(lispy PUBLIC -fsanitize=address)

include_directories(deps)