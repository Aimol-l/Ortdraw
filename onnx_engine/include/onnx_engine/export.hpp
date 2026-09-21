#pragma once
#if defined(_WIN32)
#  if defined(ONNX_ENGINE_BUILD)
#    define ONNX_ENGINE_API __declspec(dllexport)
#  else
#    define ONNX_ENGINE_API __declspec(dllimport)
#  endif
#else
#  define ONNX_ENGINE_API __attribute__((visibility("default")))
#endif
