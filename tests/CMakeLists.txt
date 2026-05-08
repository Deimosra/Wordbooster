find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(wordbooster-tests
    auto_cap_test.cpp
    dictionary_test.cpp
    history_store_test.cpp
    suggester_test.cpp
    text_utils_test.cpp
    typing_session_test.cpp
)
target_link_libraries(wordbooster-tests
    PRIVATE
        wordbooster-core
        GTest::gtest
        GTest::gtest_main
)
gtest_discover_tests(wordbooster-tests)
