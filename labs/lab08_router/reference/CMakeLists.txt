add_executable(lab08_router src/main.cpp)
target_link_libraries(lab08_router PRIVATE lab_support)
if(Python3_FOUND)
  add_test(NAME router_integration COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration.py" "$<TARGET_FILE_DIR:lab08_router>")
  set_tests_properties(router_integration PROPERTIES TIMEOUT 30)
endif()
