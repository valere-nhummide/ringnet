set(TARGET ${PROJECT_NAME}-test)

add_executable(${TARGET} src/main.cpp
    src/tcp.cpp)

target_include_directories(${TARGET}
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)


target_link_libraries(${TARGET} PRIVATE ${PROJECT_NAME} external::doctest)
