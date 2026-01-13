# Implementation Status: Universal Core

> **Status**: ✅ Phase 3b Complete (Universal Portability) + 🚧 Phase 4 (Optimization/Video)
> **Date**: 2026-01-13
> **Version**: v0.5.0

This document details the technical implementation of the A-Vision system as of the "Universal" release. It covers the architectural decisions, the specific algorithms used, and the build pipeline.

---

## 1. System Architecture: "Portable Core"

To meet the requirement of running on **any edge device** (Android, iOS, Embedded Linux), we implemented a strict separation of concerns.

### The "Core" (`src/core/`)
*   **Language**: Pure C++17.
*   **Dependencies**: OpenCV (Core, ImgProc) only. No OS-specific headers.
*   **Role**: The "Brain". It receives a generic video frame and outputs decision events.
*   **Modules**:
    *   `src/core/geometry/GeometryEngine.cpp`: Ground/Obstacle logic.
    *   `src/core/semantics/ObjectEngine.cpp`: Neural Network wrapper (OpenCV DNN).
    *   `src/core/Engine.cpp`: Main orchestration loop.

### The "Platform" (`src/platform/`)
*   **Role**: The "Body". Adapts the Core to specific hardware.
*   **Interfaces**: `ICamera.h` and `IAudio.h` define the contract.
*   **Implementations**:
    *   `desktop/DesktopCamera.cpp`: Wraps `cv::VideoCapture` for Windows/Linux webcams.
    *   `desktop/DesktopAudio.cpp`: Uses `Windows.h` Beep API for immediate, latency-free feedback.

---

## 2. Algorithms & Logic

### Ground & Obstacle Detection (`GeometryEngine.cpp`)
We prioritized **speed and determinism** over AI probability for the safety layer.
1.  **Preprocessing**: Frames are converted to Grayscale and blurred using `cv::GaussianBlur` to reduce sensor noise.
2.  **Ground Detection**: 
    *   **Algorithm**: Canny Edge Detection + Pixel Density Heuristic.
    *   **Logic**: A "safe zone" is defined at the bottom-center of the screen. We count edge pixels in this ROI. If density > 5%, the path is considered "unsafe/complex".
3.  **Obstacle Detection**:
    *   **Algorithm**: Binary Thresholding + Contour Extraction (`cv::findContours`).
    *   **Logic**: We identify dark/distinct objects. Small contours (< 1000 pixels) are filtered as noise. Remaining contours are wrapped in Bounding Boxes.

### Distance Estimation (`DistanceEngine.cpp`)
Without a depth sensor, we use **Monocular Cues (Perspective)**.
*   **Assumption**: Objects lower in the 2D frame are physically closer to the user's feet.
*   **Calculation**: `RelativeDistance = (BoxBottomY / FrameHeight)`
*   **Zones**:
    *   **IMMEDIATE**: > 85% down the screen (Critical danger).
    *   **NEAR**: > 60% down the screen (Warning).
    *   **FAR**: < 60% (Info/Ignore).

### Object Detection (`ObjectEngine.cpp`)
We upgraded to a **Generic Inference Engine** supporting multiple architectures.
*   **Models Supported**:
    *   **YOLOv8 Nano** (ONNX): High accuracy, modern architecture.
    *   **MobileNet-SSD** (V1/V2): Ultra-lightweight legacy support.
*   **Format Support**: `.onnx`, `.tflite`, `.caffemodel` (Auto-detected).
*   **Configuration**: Runtime configurable via `selected_model.json`.
    *   **Preprocessing**: Configurable Input Size (300x300, 640x640), Mean, Scale.
    *   **Post-processing**:
        *   **SSD**: Standard 4D tensor parsing.
        *   **YOLO**: Flattening, Transposition, and NMS (Non-Maximum Suppression).
*   **Integration**:
    *   **Throttling**: Inference runs every 5 frames to maintain high FPS for safety logic.
    *   **Confidence**: Thresholds defined in config (Default > 50%).

### Audio Feedback Loop
The system uses a "Silence by Default" philosophy, alerting only on positive detection.
*   **CRITICAL**: High-pitch beep (Frequency 2000Hz).
*   **WARNING**: Mid-pitch beep (Frequency 1000Hz).
*   **INFO**: Low-pitch beep (Startup/Status).

---

## 3. Build & Dependency System

We moved away from manual linking to a modern, reproducible pipeline.

*   **Package Manager**: `vcpkg` (Manifest Mode).
    *   No manual download of OpenCV required.
    *   Dependencies defined in `vcpkg.json`.
*   **Build System**: CMake (v3.14+).
    *   Links dependencies automatically.
    *   Defines the executable `AVision`.
*   **Automation**:
    *   `setup_vcpkg.bat`: Bootstraps the environment.
    *   `download_models.bat`: Fetches Neural Network weights.
    *   `build.bat`: Configures and compiles in Release mode.
    *   `clean_vcpkg.bat`: Reclaims disk space by removing intermediate objects.

---

### Phase 3: Platform Portability (`src/platform/android`)
To run on Android/iOS, we inverted the control loop.
*   **Refactor**: `Engine` is now passive (`init`, `processFrame`, `stop`). It does not own the loop.
*   **Android Bridge**: `VisionBridge.cpp` (JNI) exposes the C++ Core to Kotlin/Java.
*   **Integration**: See `ANDROID_SETUP.md` for compilation instructions on Android Studio.

### Phase 3b: Universal Extensions
We extended the portable core to support iOS and Embedded Linux.
*   **iOS**: `src/platform/ios/VisionBridge.mm` uses Objective-C++ to wrap the Engine. It handles `CVPixelBufferRef` locking/unlocking and passes `cv::Mat` to the core.
*   **Embedded**: `src/platform/embedded/HeadlessRunner.cpp` is a CLI-only application. It reuses `DesktopCamera` but disables all GUI calls (`imshow`). Designed for systemd services.

## 4. Current Limitations & Next Steps

### Limitations (v0.4.0)
*   **No Voice**: Objects are detecting but not spoken (TTS not implemented).
*   **Lighting Sensitivity**: The simple thresholding algorithm struggles in very low light or low contrast.

### Phase 4 Plan (Optimization)
*   **Voice**: Implement TTS.
*   **Performance**: Profile latency on actual mobile hardware.

