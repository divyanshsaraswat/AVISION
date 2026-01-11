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
# Removed broken target reference.

