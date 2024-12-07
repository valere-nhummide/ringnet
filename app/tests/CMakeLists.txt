set(TARGET ${PROJECT_NAME}-test)
add_executable(${TARGET} src/main.cpp)

target_link_libraries(${TARGET} PRIVATE ${PROJECT_NAME} external::doctest)
