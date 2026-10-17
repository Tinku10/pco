cmake_minimum_required(VERSION 3.15)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(pco VERSION 1.0 LANGUAGES C)

# C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_BUILD_TYPE Debug)

##### Build Static Library #####

find_path(LUAJIT_INCLUDE_DIR
    NAMES lua.h 
    PATH_SUFFIXES luajit-2.1
    REQUIRED
)

# find include dir of Node in the system
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

# Create executable and link dependencies

# Grab all .c files
file(GLOB LIB_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB LIB_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/test/*.c")

add_library(pco SHARED ${LIB_SOURCES} bindings/lua.c)
add_library(pcojs SHARED ${LIB_SOURCES} bindings/js.c)

target_compile_definitions(pco PRIVATE DEBUG)

# Include directories
target_include_directories(pco PRIVATE 
  ${CMAKE_SOURCE_DIR}/libs
  ${LUAJIT_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/src
  ${NODE_API_INCLUDE_DIR}
)

target_compile_options(pco PRIVATE -Wall -Wextra -g -O0)
target_link_options(pco PRIVATE -Wall -Wextra -g -O0)
target_link_libraries(pco PRIVATE ${LUAJIT_LIBRARY})

target_include_directories(pcojs PRIVATE 
  ${CMAKE_SOURCE_DIR}/libs
  ${LUAJIT_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/src
  ${NODE_API_INCLUDE_DIR}
)
target_compile_options(pcojs PRIVATE -Wall -Wextra -g -O0 -fsanitize=address)
# Missing symbols are loaded by Node JS runtime
target_link_options(pcojs PRIVATE -Wall -Wextra -g -O0 -undefined dynamic_lookup -fsanitize=address)
# Node js expects .node extension in shared objects
set_target_properties(pcojs PROPERTIES
  PREFIX "lib"
  SUFFIX ".node"
)


##### Executable for running Test Suite #####

# create executable directly to run tests
add_executable(pcotest ${LIB_TESTS} ${LIB_SOURCES})
target_include_directories(pcotest PRIVATE 
  ${CMAKE_SOURCE_DIR}/libs
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/test
)