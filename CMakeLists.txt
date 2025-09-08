cmake_minimum_required(VERSION 3.20)

set(PROJECT_NAME "ringnet")
project(
  ${PROJECT_NAME}
  VERSION 0.1
  LANGUAGES CXX)

# Determine whether this is a standalone project or included by other projects
set(RINGNET_STANDALONE_PROJECT OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  set(RINGNET_STANDALONE_PROJECT ON)
endif()

# Establish the project options
option(RINGNET_BUILD_SAMPLES "Set to ON to build sample" ${RINGNET_STANDALONE_PROJECT})
option(RINGNET_BUILD_TESTS "Set to ON to build tests" ${RINGNET_STANDALONE_PROJECT})

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
add_link_options($<$<STREQUAL:"${CMAKE_BUILD_TYPE}","Debug">:-rdynamic>)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O3")
add_link_options(
  $<$<STREQUAL:"${CMAKE_BUILD_TYPE}","RelWithDebInfo">:-rdynamic>)
set(CMAKE_CXX_FLAGS_RELEASE "-g -O3 -DNDEBUG")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS ON)

add_subdirectory(external)

add_compile_options(-Wall)
add_compile_options(-Weffc++)
add_compile_options(-Werror)
add_compile_options(-Werror=type-limits)
add_compile_options(-Wextra)
add_compile_options(-Wfloat-conversion)
add_compile_options(-Wformat=2)
add_compile_options(-Wno-error=deprecated-declarations)
add_compile_options(-Wno-missing-field-initializers)
add_compile_options(-Wshadow)

add_library(${PROJECT_NAME} STATIC src/ringnet/eventLoop.cpp src/ringnet/net/connection.cpp src/ringnet/net/sockets.cpp src/ringnet/uring/submissionQueue.cpp)
target_include_directories(
  ${PROJECT_NAME} PUBLIC ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(${PROJECT_NAME} PUBLIC external::expected
  external::liburing)

if(RINGNET_BUILD_SAMPLES)
  add_subdirectory(app/samples)
endif()

if(RINGNET_BUILD_TESTS)
  add_subdirectory(app/tests)
endif()
