cmake_minimum_required(VERSION 3.10.0)
project(macro VERSION 1.0.0 C)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

set(SRCS
  ${SRC_DIR}/main.c
  ${SRC_DIR}/MacroDatabase.c
)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_subdirectory(deps/glfw)

add_executable(macro ${SRCS})
target_link_libraries(macro PRIVATE glfw)

set_target_properties(macro
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)