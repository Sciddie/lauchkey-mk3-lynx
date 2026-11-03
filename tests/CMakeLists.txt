add_executable(lkmk3_tests
    test_main.cpp
    test_version.cpp
    test_protocol.cpp
)
target_link_libraries(lkmk3_tests PRIVATE lkmk3_core)
target_compile_definitions(lkmk3_tests PRIVATE LKMK3_PROJECT_VERSION="${PROJECT_VERSION}")
add_test(NAME lkmk3_tests COMMAND lkmk3_tests)
