cmake_minimum_required(VERSION 3.15)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(pco VERSION 1.0 LANGUAGES C)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug)

# add_subdirectory(libs/libuv)

# Create executable
add_executable(main
    src/main.c src/client.c src/server.c src/message.c src/fileio.c
)

# target_link_libraries(main PRIVATE uv_a)

target_compile_options(main PRIVATE -Wall -Wextra -g)

# target_link_options(main PRIVATE -fsanitize=address)

# Include directories
target_include_directories(main PRIVATE ${CMAKE_SOURCE_DIR}/libs/stb)

# # Static library
# add_library(libuv STATIC IMPORTED)

# set_target_properties(libuv PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/libuv/build/libuv.a)

# Link static library
