cmake_minimum_required(VERSION 3.15)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(pco VERSION 1.0 LANGUAGES C)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug)

# add_subdirectory(libs/libuv)

find_path(LUAJIT_INCLUDE_DIR
    NAMES lua.h 
    PATH_SUFFIXES luajit-2.1
    REQUIRED
)

find_library(LUAJIT_LIBRARY
    NAMES luajit REQUIRED
)

# Create executable

add_library(pco SHARED
    src/client.c src/server.c src/message.c src/fileio.c src/queue.c bindings/lua.c
)

target_compile_definitions(pco PRIVATE DEBUG)

# target_link_libraries(main PRIVATE uv_a)

target_compile_options(pco PRIVATE -Wall -Wextra -g -fsanitize=address -fno-omit-frame-pointer)
target_link_options(pco PRIVATE -Wall -Wextra -g -fsanitize=address -fno-omit-frame-pointer)

# target_link_options(main PRIVATE -fsanitize=address)

# Include directories
target_include_directories(pco PRIVATE 
  ${CMAKE_SOURCE_DIR}/libs/stb
  ${LUAJIT_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/src
)

add_executable(pcod
    src/main.c 
)

target_compile_definitions(pcod PRIVATE DEBUG)

target_include_directories(pcod PRIVATE 
  ${CMAKE_SOURCE_DIR}/libs/stb
  ${LUAJIT_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/src
)


target_link_libraries(pco PRIVATE
  ${LUAJIT_LIBRARY}
)

target_link_libraries(pcod PRIVATE
  pco
)

# # Static library
# add_library(libuv STATIC IMPORTED)

# set_target_properties(libuv PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/libuv/build/libuv.a)

# Link static library
