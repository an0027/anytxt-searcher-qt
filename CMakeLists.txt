cmake_minimum_required(VERSION 3.16)
project(anytxt-searcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ═══════════════════════════════════════════════════════════════
# Qt 版本探测
# ═══════════════════════════════════════════════════════════════
find_package(Qt6 QUIET COMPONENTS Widgets Concurrent)
find_package(Qt6Core5Compat QUIET)

if(Qt6_FOUND AND Qt6Core5Compat_FOUND)
    message(STATUS "✓ Using Qt6 (${Qt6_VERSION}) + Core5Compat")
    set(QT_LIBS Qt6::Widgets Qt6::Concurrent Qt6::Core5Compat)
elseif(Qt6_FOUND)
    message(STATUS "✓ Using Qt6 (${Qt6_VERSION}) - Core5Compat not found")
    set(QT_LIBS Qt6::Widgets Qt6::Concurrent)
else()
    find_package(Qt5 QUIET COMPONENTS Widgets Concurrent)
    if(NOT Qt5_FOUND)
        message(FATAL_ERROR "✗ Qt6/Qt5 not found")
    endif()
    message(STATUS "✓ Using Qt5 (${Qt5_VERSION})")
    set(QT_LIBS Qt5::Widgets Qt5::Concurrent)
endif()

# ═══════════════════════════════════════════════════════════════
# Xapian (required) - manually find to avoid vcpkg path issues with MinGW
# ═══════════════════════════════════════════════════════════════
# Try standard find first
find_path(XAPIAN_INCLUDE_DIRS xapian.h
    PATHS
        "C:/tools/vcpkg/installed/x64-windows/include"
        "$ENV{VCPKG_ROOT}/installed/x64-windows/include"
        "C:/vcpkg/installed/x64-windows/include"
        /usr/include /usr/local/include
)
find_library(XAPIAN_LIBRARIES
    NAMES xapian libxapian
    PATHS
        "C:/tools/vcpkg/installed/x64-windows/lib"
        "$ENV{VCPKG_ROOT}/installed/x64-windows/lib"
        "C:/vcpkg/installed/x64-windows/lib"
        /usr/lib /usr/local/lib
)

if(NOT XAPIAN_INCLUDE_DIRS OR NOT XAPIAN_LIBRARIES)
    message(FATAL_ERROR "✗ xapian-core not found. Install via vcpkg: vcpkg install xapian")
endif()

message(STATUS "✓ xapian-core found")
message(STATUS "  Includes: ${XAPIAN_INCLUDE_DIRS}")
message(STATUS "  Library:  ${XAPIAN_LIBRARIES}")

# ═══════════════════════════════════════════════════════════════
# Optional dependencies
# ═══════════════════════════════════════════════════════════════
set(HAS_POPPLER FALSE)
set(HAS_TESSERACT FALSE)
set(HAS_LIBZIP FALSE)

# Try pkg-config first for optional dependencies
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(POPPLER QUIET poppler-cpp)
    if(POPPLER_FOUND)
        set(HAS_POPPLER TRUE)
        message(STATUS "✓ poppler-cpp found")
    endif()
    pkg_check_modules(TESSERACT QUIET tesseract)
    if(TESSERACT_FOUND)
        set(HAS_TESSERACT TRUE)
        message(STATUS "✓ tesseract found")
    endif()
    pkg_check_modules(ZIP QUIET libzip)
    if(ZIP_FOUND)
        set(HAS_LIBZIP TRUE)
        message(STATUS "✓ libzip found")
    endif()
endif()

# If pkg-config didn't find poppler, try direct find as fallback
if(NOT HAS_POPPLER)
    find_path(POPPLER_INCLUDE_DIRS poppler/cpp/poppler-document.h
        PATHS
            "C:/tools/vcpkg/installed/x64-windows/include"
            "$ENV{VCPKG_ROOT}/installed/x64-windows/include"
    )
    find_library(POPPLER_LIBRARIES
        NAMES poppler-cpp
        PATHS
            "C:/tools/vcpkg/installed/x64-windows/lib"
            "$ENV{VCPKG_ROOT}/installed/x64-windows/lib"
    )
    if(POPPLER_INCLUDE_DIRS AND POPPLER_LIBRARIES)
        set(HAS_POPPLER TRUE)
        message(STATUS "✓ poppler-cpp found (direct path)")
        message(STATUS "  Includes: ${POPPLER_INCLUDE_DIRS}")
        message(STATUS "  Library:  ${POPPLER_LIBRARIES}")
    endif()
endif()

# Direct find fallback for tesseract
if(NOT HAS_TESSERACT)
    find_path(TESSERACT_INCLUDE_DIRS tesseract/baseapi.h
        PATHS
            "C:/tools/vcpkg/installed/x64-windows/include"
            "$ENV{VCPKG_ROOT}/installed/x64-windows/include"
    )
    find_library(TESSERACT_LIBRARIES
        NAMES tesseract libtesseract tesseract55
        PATHS
            "C:/tools/vcpkg/installed/x64-windows/lib"
            "$ENV{VCPKG_ROOT}/installed/x64-windows/lib"
    )
    if(TESSERACT_INCLUDE_DIRS AND TESSERACT_LIBRARIES)
        set(HAS_TESSERACT TRUE)
        message(STATUS "✓ tesseract found (direct path)")
        message(STATUS "  Includes: ${TESSERACT_INCLUDE_DIRS}")
        message(STATUS "  Library:  ${TESSERACT_LIBRARIES}")
    endif()
endif()

# Always look for leptonica (required by tesseract at link time)
find_path(LEPT_INCLUDE_DIRS leptonica/allheaders.h
    PATHS
        "C:/tools/vcpkg/installed/x64-windows/include"
        "$ENV{VCPKG_ROOT}/installed/x64-windows/include"
)
find_library(LEPT_LIBRARIES
    NAMES lept leptonica "leptonica-1.87.0"
    PATHS
        "C:/tools/vcpkg/installed/x64-windows/lib"
        "$ENV{VCPKG_ROOT}/installed/x64-windows/lib"
)
if(LEPT_LIBRARIES AND LEPT_INCLUDE_DIRS)
    # Check that it's really found, not a NOTFOUND placeholder
    string(REGEX MATCH "NOTFOUND" _lept_notfound "${LEPT_LIBRARIES}")
    if(NOT _lept_notfound)
        message(STATUS "✓ leptonica found")
        message(STATUS "  Library:  ${LEPT_LIBRARIES}")
    else()
        set(LEPT_LIBRARIES "")
        set(LEPT_INCLUDE_DIRS "")
    endif()
else()
    set(LEPT_LIBRARIES "")
    set(LEPT_INCLUDE_DIRS "")
endif()

# Direct find fallback for libzip
if(NOT HAS_LIBZIP)
    find_path(ZIP_INCLUDE_DIRS zip.h
        PATHS
            "C:/tools/vcpkg/installed/x64-windows/include"
            "$ENV{VCPKG_ROOT}/installed/x64-windows/include"
    )
    find_library(ZIP_LIBRARIES
        NAMES zip libzip
        PATHS
            "C:/tools/vcpkg/installed/x64-windows/lib"
            "$ENV{VCPKG_ROOT}/installed/x64-windows/lib"
    )
    if(ZIP_INCLUDE_DIRS AND ZIP_LIBRARIES)
        set(HAS_LIBZIP TRUE)
        message(STATUS "✓ libzip found (direct path)")
        message(STATUS "  Includes: ${ZIP_INCLUDE_DIRS}")
        message(STATUS "  Library:  ${ZIP_LIBRARIES}")
    endif()
endif()

# ═══════════════════════════════════════════════════════════════
# Conditional source lists
# ═══════════════════════════════════════════════════════════════
set(ANYTXT_SOURCES
    src/main.cpp
    src/core/config.cpp
    src/core/xapian_database.cpp
    src/core/xapian_indexer.cpp
    src/core/xapian_searcher.cpp
    src/core/document_processor.cpp
    src/utils/file_utils.cpp
    src/utils/string_utils.cpp
    src/parser/text_parser.cpp
    src/parser/parser_manager.cpp
    src/gui/main_window.cpp
    src/gui/search_bar.cpp
    src/gui/results_widget.cpp
    src/gui/filter_panel.cpp
    src/gui/preview_widget.cpp
    src/gui/file_panel.cpp
    src/gui/match_panel.cpp
    src/dialogs/import_dialog.cpp
    src/dialogs/export_dialog.cpp
    src/dialogs/about_dialog.cpp
    src/dialogs/watch_settings_dialog.cpp
    src/core/file_watcher.cpp
    src/core/index_queue.cpp
    src/core/rule_engine.cpp
)

if(HAS_POPPLER)
    list(APPEND ANYTXT_SOURCES src/parser/pdf_parser.cpp)
endif()
if(HAS_LIBZIP)
    list(APPEND ANYTXT_SOURCES src/parser/docx_parser.cpp)
endif()

set(ANYTXT_DEFS XAPIAN_DISABLE_QT_WARNINGS)
if(HAS_POPPLER)
    list(APPEND ANYTXT_DEFS HAS_POPPLER)
endif()
if(HAS_TESSERACT)
    list(APPEND ANYTXT_DEFS HAS_TESSERACT)
endif()
if(HAS_LIBZIP)
    list(APPEND ANYTXT_DEFS HAS_LIBZIP)
endif()

# ═══════════════════════════════════════════════════════════════
# Target setup
# ═══════════════════════════════════════════════════════════════
macro(setup_target TARGET)
    target_include_directories(${TARGET} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${XAPIAN_INCLUDE_DIRS}
        ${POPPLER_INCLUDE_DIRS}
        ${TESSERACT_INCLUDE_DIRS}
        ${ZIP_INCLUDE_DIRS}
        ${LEPT_INCLUDE_DIRS}
    )
    if(DEFINED XAPIAN_LIBRARY_DIRS)
        target_link_directories(${TARGET} PRIVATE ${XAPIAN_LIBRARY_DIRS})
    endif()
    target_link_libraries(${TARGET}
        ${QT_LIBS}
    )
    if(TARGET Xapian::Xapian)
        target_link_libraries(${TARGET} Xapian::Xapian)
    else()
        target_link_libraries(${TARGET} ${XAPIAN_LIBRARIES})
    endif()
    if(HAS_POPPLER AND DEFINED POPPLER_LIBRARIES)
        target_link_libraries(${TARGET} ${POPPLER_LIBRARIES})
    endif()
    if(HAS_TESSERACT AND DEFINED TESSERACT_LIBRARIES)
        target_link_libraries(${TARGET} ${TESSERACT_LIBRARIES})
    endif()
    if(LEPT_LIBRARIES)
        target_link_libraries(${TARGET} ${LEPT_LIBRARIES})
    endif()
    if(HAS_LIBZIP AND DEFINED ZIP_LIBRARIES)
        target_link_libraries(${TARGET} ${ZIP_LIBRARIES})
    endif()
    target_compile_definitions(${TARGET} PRIVATE ${ANYTXT_DEFS})
endmacro()

add_executable(anytxt-searcher ${ANYTXT_SOURCES} app.rc app.ico)
if(MSVC)
    target_compile_options(anytxt-searcher PRIVATE /W4 /Zc:__cplusplus)
else()
    target_compile_options(anytxt-searcher PRIVATE -Wall -Wextra)
endif()
setup_target(anytxt-searcher)

# ═══════════════════════════════════════════════════════════════
# Feature summary
# ═══════════════════════════════════════════════════════════════
message(STATUS "")
message(STATUS "═══════════════════════════════════")
message(STATUS " Build Configuration Summary")
message(STATUS "═══════════════════════════════════")
message(STATUS "  Qt:        ${Qt6_VERSION}${Qt5_VERSION}")
message(STATUS "  Xapian:    ✓ (found)")
message(STATUS "  PDF:       ${HAS_POPPLER}")
message(STATUS "  OCR:       ${HAS_TESSERACT}")
message(STATUS "  ZIP:       ${HAS_LIBZIP}")
message(STATUS "═══════════════════════════════════")

