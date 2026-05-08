# Core library — pure modules (no fcitx5 dependency), testable in isolation
add_library(wordbooster-core STATIC
    auto_cap.cpp
    dictionary.cpp
    history_store.cpp
    suggester.cpp
    text_utils.cpp
    typing_session.cpp
)
target_include_directories(wordbooster-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(wordbooster-core PUBLIC PkgConfig::SQLITE3 ICU::uc)
set_property(TARGET wordbooster-core PROPERTY POSITION_INDEPENDENT_CODE ON)

# fcitx5 addon shared library
find_package(Fcitx5Core QUIET)
if(Fcitx5Core_FOUND)
    add_library(wordbooster SHARED engine.cpp)
    set_target_properties(wordbooster PROPERTIES PREFIX "")
    target_link_libraries(wordbooster
        PRIVATE
            wordbooster-core
            Fcitx5::Core
    )
    include(GNUInstallDirs)
    install(TARGETS wordbooster
        DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/fcitx5")

    configure_file(wordbooster-addon.conf.in wordbooster.conf)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/wordbooster.conf"
        DESTINATION "${CMAKE_INSTALL_FULL_DATADIR}/fcitx5/addon")
    install(FILES "wordbooster.conf"
        DESTINATION "${CMAKE_INSTALL_FULL_DATADIR}/fcitx5/inputmethod")
else()
    message(STATUS "Fcitx5Core not found — skipping addon build (tests still available)")
endif()
