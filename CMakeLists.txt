cmake_minimum_required(VERSION 3.30)
project(
    main
    LANGUAGES CXX C
    DESCRIPTION "基于节点的深度学习图像处理工具"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

get_filename_component(PROJECT_DIR ${CMAKE_CURRENT_LIST_FILE} DIRECTORY)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_DIR}/bin)
set(CMAKE_BUILD_PARALLEL_LEVEL 4)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "current platform: Linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "current platform: Windows")
    set(OpenCV_DIR "D:/OpenCV/build/x64/vc16/lib")
    set(ONNX_DIR "D:/OnnxRuntime")
    set(TBB_DIR "D:/Tbb/lib/cmake/tbb")
    include_directories(${ONNX_DIR}/include)
    link_directories(${ONNX_DIR}/lib)
    add_compile_options(/W4 /Zc:__cplusplus /std:c++latest)
endif()

find_package(OpenCV REQUIRED)
find_package(Qt6 COMPONENTS Core Gui Quick Widgets Test REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)
link_directories(${PROJECT_DIR}/lib)
include_directories(${PROJECT_DIR}/include/)

file(GLOB SRC_FILES
    CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/*.h"
    "${PROJECT_SOURCE_DIR}/include/node/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/port/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/utils/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/command/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/command/*.h"
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

qt_add_executable(${CMAKE_PROJECT_NAME}
    WIN32
    ./assets.qrc
    ${SRC_FILES}
)
target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    ${OpenCV_LIBRARIES}
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Widgets
)

enable_testing()
add_subdirectory(tests)
