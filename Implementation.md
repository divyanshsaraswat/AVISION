# Implementation Status: Phase 1 & 2

> **Status**: ✅ Phase 2 Complete (Semantics & Object Detection)
> **Date**: 2026-01-11
> **Version**: v0.2.0

This document details the technical implementation of the A-Vision system as of the "Semantics" release. It covers the architectural decisions, the specific algorithms used, and the build pipeline.

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
We integrated semantic understanding using Lightweight Edge AI.
*   **Model**: MobileNet-SSD (Single Shot Detector).
*   **Classes**: 20 standard objects (Person, Chair, Car, Bottle, etc.).
*   **Integration**:
    *   **Throttling**: Inference runs every 5 frames to maintain high FPS for safety logic.
    *   **Confidence**: Only detections > 50% are accepted.
    *   **Visualization**: Bounding boxes and labels overlaid on debug frame.

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

## 4. Current Limitations & Next Steps

### Limitations (v0.2.0)
*   **No Voice**: Objects are detecting but not spoken (TTS not implemented).
*   **Lighting Sensitivity**: The simple thresholding algorithm struggles in very low light or low contrast.
*   **Main Thread Blocking**: Image processing and Audio happen sequentially. High-load audio could briefly stall vision.

### Phase 3 Plan (Optimization & Port)
*   **Voice**: Implement TTS for "Person detected".
*   **Android Port**: Move the core to an Android NDK project.
