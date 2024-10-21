cmake_minimum_required(VERSION 3.20)

set(PROJECT_NAME "elio")
project(
  ${PROJECT_NAME}
  VERSION 0.1
  LANGUAGES CXX)

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

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(
  ${PROJECT_NAME} INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
                            $<INSTALL_INTERFACE:include>)
target_link_libraries(${PROJECT_NAME} INTERFACE external::expected external::liburing)
add_subdirectory(app)
