include(ExternalProject)
include(FetchContent)

# Avoid warning about DOWNLOAD_EXTRACT_TIMESTAMP in CMake 3.24:
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
  cmake_policy(SET CMP0135 NEW)
endif()

# ##############################################################################
# cxxopts - Lightweight C++ option parser library.
# ##############################################################################
FetchContent_Declare(
  cxxopts
  EXCLUDE_FROM_ALL SYSTEM
  # https://github.com/jarro2783/cxxopts/releases/tag/v3.2.0
  URL ${CMAKE_CURRENT_SOURCE_DIR}/archives/cxxopts-3.2.0.tar.gz
  URL_HASH
  SHA256=9f43fa972532e5df6c5fd5ad0f5bac606cdec541ccaf1732463d8070bbb7f03b
  OVERRIDE_FIND_PACKAGE)

set(CXXOPTS_BUILD_EXAMPLES
  OFF
  CACHE BOOL "" FORCE)
set(CXXOPTS_BUILD_TESTS
  OFF
  CACHE BOOL "" FORCE)
set(CXXOPTS_ENABLE_INSTALL
  OFF
  CACHE BOOL "" FORCE)
set(CXXOPTS_ENABLE_WARNINGS
  ON
  CACHE BOOL "" FORCE)
set(CXXOPTS_USE_UNICODE_HELP
  OFF
  CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(cxxopts)
add_library(external::cxxopts ALIAS cxxopts)

# ##############################################################################
# doctest
# ##############################################################################

ExternalProject_Add(
  doctest
  PREFIX ${CMAKE_BINARY_DIR}/doctest
  URL ${CMAKE_CURRENT_SOURCE_DIR}/archives/doctest-2.4.11.tar.gz
  URL_HASH
  SHA256=632ed2c05a7f53fa961381497bf8069093f0d6628c5f26286161fbd32a560186
  UPDATE_COMMAND ""
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ""
  INSTALL_COMMAND ""
  LOG_CONFIGURE ON
  LOG_BUILD ON
  LOG_MERGED_STDOUTERR ON
  LOG_OUTPUT_ON_FAILURE ON)

# Expose required variable (DOCTEST_INCLUDE_DIR) to parent scope
ExternalProject_Get_Property(doctest source_dir)
set(DOCTEST_INCLUDE_DIR
  ${source_dir}/doctest
  CACHE INTERNAL "Path to include folder for doctest")

add_library(doctest_ INTERFACE)
add_dependencies(doctest_ doctest::doctest)
target_include_directories(doctest_ INTERFACE ${DOCTEST_INCLUDE_DIR})

add_library(external::doctest ALIAS doctest_)

# ##############################################################################
# liburing
# ##############################################################################

ExternalProject_Add(
  liburing_git
  URL ${CMAKE_CURRENT_SOURCE_DIR}/archives/liburing-liburing-2.7.tar.gz
  URL_HASH
  SHA256=56202ad443c50e684dde3692819be3b91bbe003e1c14bf5abfa11973669978c1
  CONFIGURE_COMMAND ./configure
  BUILD_COMMAND make
  BUILD_IN_SOURCE 1
  BUILD_BYPRODUCTS "<SOURCE_DIR>/src/liburing.a"
  INSTALL_COMMAND ""
  TEST_COMMAND ""
  LOG_CONFIGURE ON
  LOG_BUILD ON
  LOG_MERGED_STDOUTERR ON
  LOG_OUTPUT_ON_FAILURE ON)

ExternalProject_Get_Property(liburing_git SOURCE_DIR)

add_library(liburing INTERFACE)
add_dependencies(liburing liburing_git)
target_include_directories(liburing INTERFACE ${SOURCE_DIR}/src/include)

target_link_libraries(liburing INTERFACE "${SOURCE_DIR}/src/liburing.a")

add_library(external::liburing ALIAS liburing)

# ##############################################################################
# tl::expected
# ##############################################################################

FetchContent_Declare(
  tl_expected
  URL ${CMAKE_CURRENT_SOURCE_DIR}/archives/expected-1.1.0.tar.gz
  URL_HASH
  SHA256=1db357f46dd2b24447156aaf970c4c40a793ef12a8a9c2ad9e096d9801368df6)

set(EXPECTED_BUILD_TESTS
  OFF
  CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(tl_expected)
add_library(external::expected ALIAS expected)


if(ELIO_BUILD_SAMPLES)
  FetchContent_Declare(asio SYSTEM URL ${CMAKE_CURRENT_SOURCE_DIR}/archives/asio-asio-1-36-0.tar.gz)
  FetchContent_MakeAvailable(asio)

  add_library(asio STATIC ${asio_SOURCE_DIR}/asio/src/asio.cpp)
  target_compile_definitions(asio PUBLIC ASIO_SEPARATE_COMPILATION ASIO_NO_DEPRECATED ASIO_HAS_IO_URING ASIO_DISABLE_EPOLL)
  target_include_directories(asio SYSTEM PUBLIC ${asio_SOURCE_DIR}/include)
  target_link_libraries(asio PUBLIC external::liburing)
  add_library(external::asio ALIAS asio)
endif()
