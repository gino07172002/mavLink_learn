add_executable(signing_tests src/main.cpp)
target_link_libraries(signing_tests PRIVATE lab_support)
add_test(NAME signing COMMAND signing_tests)
