cmake_minimum_required(VERSION 4.0.0)
project(macro LANGUAGES C VERSION 0.0.1)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(SRCS
  ${SRC_DIR}/main.c
  ${SRC_DIR}/MacroDatabase.c
)

set(FREETYPE "${CMAKE_SOURCE_DIR}/deps/freetype-2.13.3")
set(GLAD_SRC_DIR ${CMAKE_SOURCE_DIR}/deps/glad/src)
set (DEP_SRCS
  ${GLAD_SRC_DIR}/gl.c
  ${GLAD_SRC_DIR}/vulkan.c

  ${CMAKE_SOURCE_DIR}/deps/log.c/src/log.c
)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/glfw)

list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/deps")

add_executable(macro ${SRCS} ${DEP_SRCS})

target_include_directories(macro PRIVATE
  ${CMAKE_SOURCE_DIR}/include

  ${CMAKE_SOURCE_DIR}/deps/glad/include
  ${CMAKE_SOURCE_DIR}/deps/cglm/include
  ${FREETYPE}/include
  ${CMAKE_SOURCE_DIR}/deps/log.c/src
  ${CMAKE_SOURCE_DIR}/deps/stb
)

target_link_libraries(macro PRIVATE glfw "${FREETYPE}/objs/x64/Debug Static/freetype.lib")

set_target_properties(macro
  PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)