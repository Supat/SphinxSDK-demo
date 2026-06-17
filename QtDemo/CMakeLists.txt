cmake_minimum_required(VERSION 3.16)
project(SphinxQtDemo LANGUAGES CXX)

# v140 (VS2015) -> use a Qt 5 build for msvc2015/2017. Qt 6 needs MSVC 2019+.
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Prefer Qt6 if present, fall back to Qt5. Set Qt5_DIR / CMAKE_PREFIX_PATH to
# your Qt install (e.g. -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2015_64).
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

# Reuse the FPN / Bayer helpers and the vendored SDK header from the console demo.
set(FPN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ConsoleDemo_FPN")
set(SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../SphinxLib")

add_executable(SphinxQtDemo WIN32
    main.cpp
    Camera.cpp        Camera.h
    MainWindow.cpp    MainWindow.h
    "${FPN_DIR}/darknoise.cpp"
    "${FPN_DIR}/bayer.cpp"
)

target_include_directories(SphinxQtDemo PRIVATE "${FPN_DIR}" "${SDK_DIR}")

# Pick the matching SDK import library per configuration.
set(SPHINX_LIB
    "$<IF:$<CONFIG:Debug>,${SDK_DIR}/Debug64/SphinxLib.lib,${SDK_DIR}/Release64/SphinxLib.lib>")

target_link_libraries(SphinxQtDemo PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    "${SPHINX_LIB}"
    Ws2_32
    IPHlpApi
)

# Copy the SDK runtime next to the binary so it can be launched directly.
set(SPHINX_DLL
    "$<IF:$<CONFIG:Debug>,${SDK_DIR}/Debug64/SphinxLib.dll,${SDK_DIR}/Release64/SphinxLib.dll>")

add_custom_command(TARGET SphinxQtDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SPHINX_DLL}" "$<TARGET_FILE_DIR:SphinxQtDemo>"
    COMMENT "Copying SphinxLib.dll")

# The SDK's XML/runtime helpers (libxml2, libxslt, MathParser, schemas) ship in
# Extras64 and must sit beside the exe too.
if(EXISTS "${SDK_DIR}/Extras64")
    add_custom_command(TARGET SphinxQtDemo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SDK_DIR}/Extras64" "$<TARGET_FILE_DIR:SphinxQtDemo>"
        COMMENT "Copying SDK Extras64 runtime files")
endif()

# Deploy the Qt runtime DLLs (windeployqt) for Release builds on Windows.
if(WIN32)
    get_target_property(_qmake Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin "${_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT windeployqt HINTS "${_qt_bin}")
    if(WINDEPLOYQT)
        add_custom_command(TARGET SphinxQtDemo POST_BUILD
            COMMAND "${WINDEPLOYQT}" --no-translations "$<TARGET_FILE:SphinxQtDemo>"
            COMMENT "Deploying Qt runtime")
    endif()
endif()
