cmake_minimum_required(VERSION 3.14)
project(AVision)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenCV package
find_package(OpenCV REQUIRED)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/src
)

# Define source files
set(CORE_SOURCES
    src/core/Engine.cpp
    src/core/geometry/GeometryEngine.cpp
    src/core/distance/DistanceEngine.cpp
    src/core/semantics/ObjectEngine.cpp
    src/core/semantics/TemporalModule.cpp
    src/core/depth/DepthEngine.cpp
    src/core/geometry/FreeSpaceModule.cpp
    src/core/geometry/EdgeSafetyModule.cpp
    src/core/semantics/OCRModule.cpp
    src/core/geometry/PathGuidanceModule.cpp
    src/core/semantics/SceneUnderstandingModule.cpp
    src/core/utils/ConfigLoader.cpp
    src/core/segmentation/SegmentationModule.cpp
    src/core/segmentation/MobileSAMBackend.cpp
    src/core/segmentation/ClassicalBackend.cpp
    src/core/filling/FillModule.cpp
    src/core/filling/LaMaBackend.cpp
)

set(PLATFORM_SOURCES
    src/platform/desktop/DesktopCamera.cpp
    src/platform/desktop/DesktopAudio.cpp
)

# Find ONNX Runtime
find_package(onnxruntime CONFIG REQUIRED)

add_executable(AVision
    src/main.cpp
    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_libraries(AVision ${OpenCV_LIBS} onnxruntime::onnxruntime)

# ... (omitted copy commands)

add_executable(AVisionCLI
    src/video_cli.cpp
    src/ImageCLI.cpp
    src/core/Engine.cpp
    src/core/semantics/ObjectEngine.cpp
    src/core/semantics/TemporalModule.cpp
    src/core/geometry/GeometryEngine.cpp
    src/core/geometry/FreeSpaceModule.cpp
    src/core/geometry/EdgeSafetyModule.cpp
    src/core/semantics/OCRModule.cpp
    src/core/geometry/PathGuidanceModule.cpp
    src/core/semantics/SceneUnderstandingModule.cpp
    src/core/distance/DistanceEngine.cpp
    src/core/depth/DepthEngine.cpp
    src/platform/desktop/DesktopAudio.cpp
    src/core/utils/ConfigLoader.cpp
    src/core/segmentation/SegmentationModule.cpp
    src/core/segmentation/MobileSAMBackend.cpp
    src/core/segmentation/ClassicalBackend.cpp
    src/core/filling/FillModule.cpp
    src/core/filling/LaMaBackend.cpp
)
target_link_libraries(AVisionCLI ${OpenCV_LIBS} onnxruntime::onnxruntime)

# --- Configuration Tool (TUI) ---
find_package(ftxui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_executable(AVisionConfig src/tools/ConfigTool.cpp)
target_link_libraries(AVisionConfig PRIVATE 
    ftxui::screen
    ftxui::dom
    ftxui::component
    nlohmann_json::nlohmann_json
)


