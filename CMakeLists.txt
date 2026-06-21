cmake_minimum_required(VERSION 3.16)
project(lineart_auto_drawer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 平台默认开关
option(BUILD_WITH_X11 "Build Linux X11 mouse controller" ${UNIX})
option(BUILD_WITH_WINAPI "Build Windows mouse controller" ${WIN32})

# --- 依赖 --------------------------------------------------------------------
find_package(OpenCV REQUIRED)

# 检测 potrace（Linux 通过 pkg-config，其他平台通过 find_path/find_library / VCPKG）
set(HAVE_POTRACE OFF)
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(POTRACE QUIET potrace)
    if(POTRACE_FOUND)
        set(HAVE_POTRACE ON)
    endif()
endif()
if(NOT HAVE_POTRACE)
    find_path(POTRACE_INCLUDE_DIR potracelib.h)
    find_library(POTRACE_LIBRARY NAMES potrace potracelib)
    if(POTRACE_INCLUDE_DIR AND POTRACE_LIBRARY)
        set(HAVE_POTRACE ON)
        set(POTRACE_INCLUDE_DIRS ${POTRACE_INCLUDE_DIR})
        set(POTRACE_LIBRARIES ${POTRACE_LIBRARY})
    endif()
endif()

set(LINEART_SOURCES
    src/image_preprocessor.cpp
    src/line_extractor.cpp
    src/svg_exporter.cpp
    src/mouse_controller.cpp
    src/batch_processor.cpp
)

set(LINEART_HEADERS
    include/image_preprocessor.h
    include/line_extractor.h
    include/svg_exporter.h
    include/mouse_controller.h
    include/batch_processor.h
    include/common.h
)

add_library(lineart_core STATIC ${LINEART_SOURCES} ${LINEART_HEADERS})
target_include_directories(lineart_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)
target_link_libraries(lineart_core PUBLIC ${OpenCV_LIBS})

if(HAVE_POTRACE)
    target_include_directories(lineart_core PUBLIC ${POTRACE_INCLUDE_DIRS})
    target_link_libraries(lineart_core PUBLIC ${POTRACE_LIBRARIES})
    target_compile_definitions(lineart_core PUBLIC HAVE_POTRACE=1)
    message(STATUS "libpotrace found: will use it for vectorization")
else()
    message(STATUS "libpotrace not found: falling back to built-in polyline SVG exporter")
endif()

if(UNIX AND BUILD_WITH_X11)
    find_package(X11)
    if(X11_FOUND)
        target_compile_definitions(lineart_core PUBLIC LINUX_MOUSE=1)
        target_link_libraries(lineart_core PUBLIC ${X11_LIBRARIES} Xtst)
        target_include_directories(lineart_core PUBLIC ${X11_INCLUDE_DIR})
        message(STATUS "X11 found: Linux mouse control enabled")
    else()
        message(STATUS "X11 not found: mouse control disabled")
    endif()
endif()

if(WIN32 AND BUILD_WITH_WINAPI)
    target_compile_definitions(lineart_core PUBLIC WINDOWS_MOUSE=1)
    target_link_libraries(lineart_core PUBLIC user32)
    message(STATUS "Windows mouse control enabled")
endif()

add_executable(lineart_drawer src/main.cpp)
target_link_libraries(lineart_drawer PRIVATE lineart_core)
if(WIN32)
    set_target_properties(lineart_drawer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()
