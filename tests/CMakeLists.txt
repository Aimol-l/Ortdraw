set(TEST_HEADERS
    ${PROJECT_SOURCE_DIR}/include/Theme.h
    ${PROJECT_SOURCE_DIR}/include/Settings.h
    ${PROJECT_SOURCE_DIR}/include/NodeManager.h
    ${PROJECT_SOURCE_DIR}/include/PaintBoard.h
    ${PROJECT_SOURCE_DIR}/include/Log.hpp
    ${PROJECT_SOURCE_DIR}/include/node/BaseNode.hpp
    ${PROJECT_SOURCE_DIR}/include/node/ImageLoad.hpp
    ${PROJECT_SOURCE_DIR}/include/node/ImageShow.hpp
    ${PROJECT_SOURCE_DIR}/include/node/ImageSave.hpp
    ${PROJECT_SOURCE_DIR}/include/node/Resize.hpp
    ${PROJECT_SOURCE_DIR}/include/node/Blur.hpp
    ${PROJECT_SOURCE_DIR}/include/node/Median.hpp
    ${PROJECT_SOURCE_DIR}/include/node/Morphology.hpp
    ${PROJECT_SOURCE_DIR}/include/node/Blend.hpp
    ${PROJECT_SOURCE_DIR}/include/node/Threshold.hpp
    ${PROJECT_SOURCE_DIR}/include/node/Gray.hpp
    ${PROJECT_SOURCE_DIR}/include/node/EdgeDetect.hpp
    ${PROJECT_SOURCE_DIR}/include/node/Crop.hpp
    ${PROJECT_SOURCE_DIR}/include/node/FlipRotate.hpp
    ${PROJECT_SOURCE_DIR}/include/node/BrightnessContrast.hpp
    ${PROJECT_SOURCE_DIR}/include/node/Conv.hpp
    ${PROJECT_SOURCE_DIR}/include/node/Tensor.hpp
    ${PROJECT_SOURCE_DIR}/include/node/PreProcess.hpp
    ${PROJECT_SOURCE_DIR}/include/node/PostProcess.hpp
    ${PROJECT_SOURCE_DIR}/include/node/OnnxInfer.hpp
    ${PROJECT_SOURCE_DIR}/include/engine/executors/TensorExecutor.hpp
    ${PROJECT_SOURCE_DIR}/include/engine/executors/PreProcessExecutor.hpp
    ${PROJECT_SOURCE_DIR}/include/engine/executors/OnnxInferExecutor.hpp
    ${PROJECT_SOURCE_DIR}/include/engine/executors/PostProcessExecutor.hpp
    ${PROJECT_SOURCE_DIR}/include/engine/tasks/TaskSpec.hpp
    ${PROJECT_SOURCE_DIR}/include/engine/tasks/PreProcessRegistry.hpp
    ${PROJECT_SOURCE_DIR}/include/engine/tasks/PostProcessRegistry.hpp
    ${PROJECT_SOURCE_DIR}/include/engine/onnx/OnnxTensorConvert.hpp
    ${PROJECT_SOURCE_DIR}/include/ExecQueueModel.hpp
    ${PROJECT_SOURCE_DIR}/include/QueueFilterProxyModel.hpp
    ${PROJECT_SOURCE_DIR}/include/engine/NodeStatus.hpp
    ${PROJECT_SOURCE_DIR}/include/port/Port.hpp
    ${PROJECT_SOURCE_DIR}/include/utils/DAGraph.hpp
    ${PROJECT_SOURCE_DIR}/include/utils/Edge.hpp
    ${PROJECT_SOURCE_DIR}/include/utils/Snapshot.hpp
    ${PROJECT_SOURCE_DIR}/include/utils/QueueGroups.hpp
    ${PROJECT_SOURCE_DIR}/include/engine/GraphExecutor.hpp
    ${PROJECT_SOURCE_DIR}/include/command/Command.h
    ${PROJECT_SOURCE_DIR}/include/command/AddNode.hpp
    ${PROJECT_SOURCE_DIR}/include/command/AddEdge.hpp
    ${PROJECT_SOURCE_DIR}/include/command/RemoveNode.hpp
    ${PROJECT_SOURCE_DIR}/include/command/RemoveEdge.hpp
    ${PROJECT_SOURCE_DIR}/include/command/CmdManager.hpp
    ${PROJECT_SOURCE_DIR}/include/command/MoveNodeCMD.hpp
    ${PROJECT_SOURCE_DIR}/include/command/ResizeNodeCMD.hpp
    ${PROJECT_SOURCE_DIR}/include/command/ChangeParamsCMD.hpp
)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")
foreach(src ${TEST_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    qt_add_executable(${name} ${src} ${TEST_HEADERS})
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
    )
    target_compile_features(${name} PRIVATE cxx_std_23)
    target_compile_definitions(${name} PRIVATE
        ORTDRAW_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    )
    target_link_libraries(${name} PRIVATE
        Qt6::Core Qt6::Gui Qt6::Quick Qt6::Test
        ${OpenCV_LIBRARIES}
        Tensorvia::tensorvia
        onnx_engine
    )
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()
