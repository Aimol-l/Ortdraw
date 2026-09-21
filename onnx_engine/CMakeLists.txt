cmake_minimum_required(VERSION 3.30)
project(onnx_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- onnxruntime 探测（硬依赖）----
if(NOT ONNXRUNTIME_ROOT)
    set(ONNXRUNTIME_ROOT "/usr" CACHE PATH "onnxruntime 安装前缀")
endif()
find_library(ONNXRUNTIME_LIB NAMES onnxruntime
    PATHS "${ONNXRUNTIME_ROOT}/lib" "${ONNXRUNTIME_ROOT}/lib64" NO_DEFAULT_PATH)
find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
    PATHS "${ONNXRUNTIME_ROOT}/include/onnxruntime" NO_DEFAULT_PATH)
if(NOT ONNXRUNTIME_LIB OR NOT ONNXRUNTIME_INCLUDE_DIR)
    message(FATAL_ERROR
        "未找到 onnxruntime。请安装（如 pacman -S onnxruntime-opt）或用 -DONNXRUNTIME_ROOT=<prefix> 指定。")
endif()
message(STATUS "onnxruntime: ${ONNXRUNTIME_LIB}")

add_library(onnx_engine SHARED
    src/runtime.cpp
)
target_include_directories(onnx_engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_include_directories(onnx_engine PRIVATE
    "${ONNXRUNTIME_INCLUDE_DIR}"
)
target_compile_definitions(onnx_engine PRIVATE ONNX_ENGINE_BUILD)
target_link_libraries(onnx_engine PRIVATE "${ONNXRUNTIME_LIB}")
set_target_properties(onnx_engine PROPERTIES
    VERSION 0.1.0
    SOVERSION 0
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
)
