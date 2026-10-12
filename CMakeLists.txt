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

# find include dir of Node
find_program(NODE_EXECUTABLE NAMES node node.exe REQUIRED)

execute_process(
    COMMAND "${NODE_EXECUTABLE}" -p "process.execPath"
    OUTPUT_VARIABLE NODE_EXEC_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)

get_filename_component(NODE_BIN_DIR "${NODE_EXEC_PATH}" DIRECTORY)
get_filename_component(NODE_PREFIX "${NODE_BIN_DIR}" DIRECTORY)

find_path(NODE_API_INCLUDE_DIR
    NAMES node_api.h 
    PATHS
      "${NODE_PREFIX}/include/node"
      "${NODE_PREFIX}/include"
    REQUIRED
)

find_library(LUAJIT_LIBRARY
    NAMES luajit REQUIRED
)

# Create executable

add_library(pco SHARED
    src/lib.c src/client.c src/server.c src/message.c src/fileio.c src/queue.c src/diff.c bindings/lua.c
)

add_library(pcojs SHARED
    src/lib.c src/client.c src/server.c src/message.c src/fileio.c src/queue.c src/diff.c bindings/js.c
)

target_compile_definitions(pco PRIVATE DEBUG)

# target_link_libraries(main PRIVATE uv_a)

target_compile_options(pco PRIVATE -Wall -Wextra -g -O0)
target_link_options(pco PRIVATE -Wall -Wextra -g -O0)

target_compile_options(pcojs PRIVATE -Wall -Wextra -g -O0)
target_link_options(pcojs PRIVATE -Wall -Wextra -g -O0)

# target_link_options(main PRIVATE -fsanitize=address)

# Include directories
target_include_directories(pco PRIVATE 
  ${CMAKE_SOURCE_DIR}/libs
  ${LUAJIT_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/src
  ${NODE_API_INCLUDE_DIR}
)

target_include_directories(pcojs PRIVATE 
  ${CMAKE_SOURCE_DIR}/libs
  ${LUAJIT_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/src
  ${NODE_API_INCLUDE_DIR}
)

add_executable(pcod
    src/main.c 
)

target_compile_definitions(pcod PRIVATE DEBUG)

target_include_directories(pcod PRIVATE 
  ${CMAKE_SOURCE_DIR}/libs
  ${LUAJIT_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/src
  ${NODE_API_INCLUDE_DIR}
)

target_link_libraries(pco PRIVATE
  ${LUAJIT_LIBRARY}
)

target_link_libraries(pcod PRIVATE
  pco
)

target_compile_definitions(pcojs PRIVATE DEBUG)

# Node js expects .node extension in shared objects
set_target_properties(pcojs PROPERTIES
  PREFIX "lib"
  SUFFIX ".node"
)

# Missing symbols are loaded by Node JS runtime
target_link_options(pcojs PRIVATE
  "-undefined" "dynamic_lookup"
)

# # Static library
# add_library(libuv STATIC IMPORTED)

# set_target_properties(libuv PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/libuv/build/libuv.a)

# Link static library
