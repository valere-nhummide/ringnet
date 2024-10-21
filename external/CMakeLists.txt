include(ExternalProject)
include(FetchContent)

# ##############################################################################
# Liburing
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
    LOG_OUTPUT_ON_FAILURE ON
)

ExternalProject_Get_Property(liburing_git SOURCE_DIR)

add_library(liburing INTERFACE)
add_dependencies(liburing liburing_git)
target_include_directories(liburing INTERFACE
    ${SOURCE_DIR}/src/include
)

target_link_libraries(liburing INTERFACE
    "${SOURCE_DIR}/src/liburing.a"
)

add_library(external::liburing ALIAS liburing)

# ##############################################################################
# tl::expected
# ##############################################################################

FetchContent_Declare(
    tl_expected
    URL ${CMAKE_CURRENT_SOURCE_DIR}/archives/expected-1.1.0.tar.gz
    URL_HASH
            SHA256=1db357f46dd2b24447156aaf970c4c40a793ef12a8a9c2ad9e096d9801368df6
)
FetchContent_MakeAvailable(tl_expected)
add_library(external::expected ALIAS expected)
