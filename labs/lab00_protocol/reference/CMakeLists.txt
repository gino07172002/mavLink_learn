add_executable(protocol_tests src/main.cpp)
target_link_libraries(protocol_tests PRIVATE lab_support lab04_model)
add_test(NAME protocol COMMAND protocol_tests)
