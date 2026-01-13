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
)

set(PLATFORM_SOURCES
    src/platform/desktop/DesktopCamera.cpp
    src/platform/desktop/DesktopAudio.cpp
)

add_executable(AVision
    src/main.cpp
    ${CORE_SOURCES}
    ${PLATFORM_SOURCES}
)

target_link_libraries(AVision ${OpenCV_LIBS})

# Copy DLLs handles by vcpkg automatically in many cases, or manual copy needed for release.

# Copy models directory to output (VS creates subfolders like Debug/Release)
add_custom_command(TARGET AVision POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/models
    $<TARGET_FILE_DIR:AVision>/models
    COMMENT "Copying models to output directory"
)

add_custom_command(TARGET AVision POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/download_models.bat
    $<TARGET_FILE_DIR:AVision>/download_models.bat
    COMMENT "Copying setup script to output directory"
)

add_executable(AVisionCLI
    src/video_cli.cpp
    src/core/semantics/ObjectEngine.cpp
)
target_link_libraries(AVisionCLI ${OpenCV_LIBS})



