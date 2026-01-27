# A-Vision (Assistive Vision System)

![Version](https://img.shields.io/badge/version-v0.5.0-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20Android%20%7C%20iOS-lightgrey)

<img width="2304" height="1856" alt="A-Vision (Assistive Vision System)" src="https://github.com/user-attachments/assets/839f25f2-71ff-4d0a-83b8-d9ddcb70f359" />

**A-Vision** is a high-performance, cross-platform C++ engine designed to assist visually impaired users. Built with **Modern C++17**, **CMake**, and **OpenCV**, designed to run on low-power edge devices.

**Codebase Status**: ✅ Phase 3b Complete (Universal Portability) + Phase 4 (Optimization/Video) + Phase 6 (Advanced Safety)

---

## Table of Contents
- [📊 Performance Benchmarks (v1.2.0)](#-performance-benchmarks-v120)
- [🚀 Getting Started](#-getting-started)
- [🧠 System Architecture: "Portable Core"](#-system-architecture-portable-core)
- [🔬 Deep Dive: Algorithms & Logic](#-deep-dive-algorithms--logic)
- [🛠️ Build & Dependency System](#%EF%B8%8F-build--dependency-system)
- [📂 Project Structure](#-project-structure)
- [🌍 Platform Support Status](#-platform-support-status)
- [🔮 Roadmap](#-roadmap)

---

## 📊 Performance Benchmarks (v1.2.0)

| Metric | Value | Description |
| :--- | :--- | :--- |
| **Average Effective FPS** | **37.3 FPS** | User Perception Rate. |
| **Skip Ratio** | **80.4%** | Frames optimized via Execution Gates. |
| **P95 Latency (AI)** | **99.8 ms** | Max inference latency (ObjectModule). |
| **Memory Peak** | **594 MB** | Max RAM usage. |

*Tested on Windows 11 x64.*

---

## 🚀 Getting Started

### Prerequisites
*   **Git**: Version control.
*   **CMake**: Build system generator (v3.14+).
*   **C++ Compiler**: MSVC (Visual Studio 2022) on Windows, or GCC/Clang on Linux.

> **Note**: You do NOT need to manually install OpenCV or ONNX Runtime. The project handles dependencies automatically via `vcpkg`.

### 🛠️ One-Click Setup (Windows)

1.  **Initialize Dependencies**:
    Run the setup script **once** to install the package manager.
    ```cmd
    .\setup_vcpkg.bat
    ```

2.  **Download AI Models (Phase 2)**:
    Required for Object Detection.
    ```cmd
    .\download_models.bat
    ```
    *Note: Models are also automatically checked and downloaded during the build process.*

3.  **Build the System**:
    This script configures CMake, downloads OpenCV (if needed), and compiles the Release build.
    ```cmd
    .\build.bat
    ```
    *First run may take 10-15 minutes to compile OpenCV. Subsequent builds are instant.*

4.  **Run the Application**:
    ```cmd
    build\Release\AVision.exe
    ```

### 🎥 Video Analysis (New)
You can now process video files directly using the new CLI tool. This output includes **Object Detection** fused with **Depth Estimation**.

```cmd
build\Release\AVisionCLI.exe --video "path/to/video.mp4"
```

### 🖼️ Image Tools (v1.2)
A set of **Deterministic & Classical** tools for image manipulation. No generative AI, always reproducible.

**1. Segmentation (ROI or Smart Click)**
```cmd
# Option A: Interactive "Smart Click" (Recommended)
build\Release\AVisionCLI.exe image segment --input raw.jpg --interactive --output mask.png

**Controls:**
*   **Left Click**: Add **Foreground** Point (Green).
*   **Right Click**: Add **Background** Point (Red).
*   **Left Drag**: Draw **Bounding Box** (Blue). You can draw multiple boxes.
*   **Space / Enter**: Run Segmentation.
*   **'c'**: Clear all prompts.
*   **'s'**: Save mask & exit.

# Option B: Explicit Bounding Box
build\Release\AVisionCLI.exe image segment --input raw.jpg --bbox 100,100,200,200 --output mask.png
```
*   **Method**: `MobileSAM` (AI-based Segment Anything Model) via **ONNX Runtime**.
*   **Architecture**: Encoder (ViT) + Decoder (Transformer).
*   **Fallback**: `cv::grabCut` (if models missing).
*   **Interaction**: Supports **Smart Click** (AI) or **Bounding Box**.

**2. Generative Fill (AI Inpainting)**
```cmd
build\Release\AVisionCLI.exe image fill --input raw.jpg --mask mask_mask.png --output filled.jpg --padding 50
```
*   **Method**: `Big-LaMa` (Large Mask Inpainting) via **ONNX Runtime**.
*   **Goal**: Hallucinate realistic background to fill missing regions.
*   **Input**: Requires a **Binary Mask** (White=Fill, Black=Keep). The `segment` command above automatically generates this as `[output]_mask.png`.
*   **Fallback**: `cv::inpaint` (Telea/Navier-Stokes) if model is missing.

**Features:**
*   **Dual-Rate Processing**: Detects objects at ~10 FPS, estimates depth at 1 FPS (Optimized).
*   **Depth Heatmap**: Visualizes user's distance to environment.
*   **Safety Fusion**: Color-codes objects (Red/Green) based on physical proximity.

### 🧹 Disk Space Management
Building from source creates large intermediate files (~8GB). To reclaim space after a successful build:
```cmd
.\clean_vcpkg.bat
```
*This keeps the libraries but removes the source code and build objects.*

---

## 🧠 System Architecture: "Portable Core"

This project follows a strict **Systems Engineering** approach to ensure safety and portability. The code is divided into two distinct layers:

### 1. The Core ("The Brain")
*   **Location**: `src/core/`
*   **Dependencies**: Pure C++ STL, OpenCV (Core/ImgProc). **NO OS dependencies.**
*   **Responsibility**:
    *   **Graph Execution Engine**: Dynamic pipeline management using DAG (Directed Acyclic Graph) & Topological Sorting.
    *   **Geometry Engine**: Analyses the frame to find walkable ground and obstacles.
    *   **Distance Engine**: Estimates "Time-to-Collision" using monocular cues.
    *   **Risk Engine**: Decides *if* and *when* to alert the user (Safety Logic).
    *   **Object Engine**: Neural Network wrapper (OpenCV DNN).
    *   **Temporal Module**: Stabilizes detections over time to reduce jitter.
    *   **Free Space Module**: Analyzes depth maps to identify safe navigation sectors.
    *   **Edge Safety Module**: Detects drop-offs and curbs using depth gradients.
*   **Portability**: Compiles unchanged on Windows, Linux, Android (NDK), iOS, and Raspberry Pi.

### 2. The Platform ("The Body")
*   **Location**: `src/platform/`
*   **Dependencies**: OS-specific APIs (Windows SDK, Android CameraX, ALSA, etc.).
*   **Responsibility**:
    *   Captures frames and feeds them to the Core.
    *   Plays audio alerts triggered by the Core.
*   **Current Implementation**: `desktop/` (Windows/Linux compatible via OpenCV/Console).

---

## 🔬 Deep Dive: Algorithms & Logic

### 1. The "Graph Engine" (Core Architecture)
As of v0.5.0, A-Vision runs on a **Graph-Based Execution Engine**, allowing for flexible reconfiguration without recompiling.
*   **Configuration (`models/modules.json`)**: Defines `nodes` (modules) and `edges` (dependencies).
*   **Topological Sort**: The Engine uses Kahn's Algorithm to flatten the DAG into a linear execution queue, ensuring dependencies are respected (e.g., `Object -> Temporal`).
*   **Execution Gates (Phase 3)**:
    *   **Logic**: `If SourceModule.confidence > Threshold -> Skip TargetModule`.
    *   **Goal**: Dynamic pruning. For example, if `GeometryModule` is 100% unsure about the path, we can skip the heavy `ObjectModule` to save power, or vice versa.
    *   **Example**: `If Geometry > 0.85 (Safe) -> Skip Object Detection`.

### 2. Unified Metrics Layer
We implement a transparent observability layer to solve the "Measurement Illusion".
*   **Scheduler FPS**: The raw tick rate of the loop (e.g., 600Hz).
*   **Effective FPS**: The rate of *heavy* module execution (e.g., 15Hz).
*   **Confidence Scores**: Every module reports a `0.0-1.0` confidence metric, enabling dynamic downstream logic.
*   **Heavy Module Tracking**: Logs determining *why* a frame was slow (e.g., `["ObjectModule", "DepthModule"]`).

### 3. Object Engine (`src/core/semantics/ObjectEngine.cpp`)
The "Semantic Eye" of the system. It wraps OpenCV DNN to provide generic object detection.

*   **Model Support**: Factory Pattern based on `ModelType` enum (`SSD_MOBILENET`, `YOLO_V8`).
*   **Configuration**: Runtime configurable via `modules.json`.
    *   **Input Resolution**: SSD (300x300), YOLO (640x640).
    *   **Normalization**: Auto-configured based on model type.
*   **Post-Processing**:
    *   **SSD**: `confidence > scoreThreshold`.
    *   **YOLOv8**: `NMSBoxes` (Non-Maximum Suppression) with `nmsThreshold=0.45` to remove duplicate boxes.

### 2. Geometry Engine (`src/core/geometry/GeometryEngine.cpp`)
The "Safety Layer". Uses classical Computer Vision (no AI) for determinism and speed.

*   **Ground Detection (Safe Zone)**:
    *   **Goal**: Determine if the floor immediately in front of the user is walkable.
    *   **Algorithm**: Canny Edge Detection + Pixel Density Heuristic.
    *   **Logic**: A "safe zone" is defined at the bottom-center of the screen. We count edge pixels in this ROI. If density > 5%, the path is considered "unsafe/complex" (e.g., stairs, rubble).
*   **Obstacle Detection**:
    *   **Goal**: Find physical objects blocking the path.
    *   **Algorithm**: `Grayscale` -> `GaussianBlur` -> `Inverse Binary Threshold` -> `findContours`.
    *   **Filtering**: Ignores noise (< 1000 pixels) and distant objects (top half of screen).

### 3. Distance Engine (`src/core/distance/DistanceEngine.h`)
Calculates "Time-to-Collision" based on 2D position (Monocular Depth).

*   **Technique**: "Horizon Line Assumption".
*   **Formula**: `RelativeDistance = (BoxBottomY / FrameHeight)`.
*   **Zones**:
    *   **IMMEDIATE (Danger)**: `> 0.85` (Bottom 15% of screen). Triggers "CRITICAL STOP".
    *   **NEAR (Warning)**: `> 0.60` (Bottom 40% of screen). Triggers "WARNING".
    *   **FAR**: `< 0.60`. Passive tracking only.

### 4. Advanced Safety Modules (New in Phase 6)
*   **Temporal Stabilization (`TemporalModule.cpp`)**:
    *   **Goal**: Reduce jitter in object detection.
    *   **Logic**: Tracks objects across frames using IoU. Smooths bounding box coordinates (window size 5).
*   **Free Space Navigation (`FreeSpaceModule.cpp`)**:
    *   **Goal**: Find safe directions to walk.
    *   **Logic**: Splits depth map into 3 sectors (Left, Center, Right). Checks average depth against `minClearance` (1.0).
*   **Edge/Drop-Off Detection (`EdgeSafetyModule.cpp`)**:
    *   **Goal**: Detect stairs, curbs, and holes.
    *   **Logic**: Computes Y-axis gradient of depth map (sobels). High gradient > `0.25` indicates sharp elevation change.

### 6. Semantic & Guidance Modules (New in Phase 6b)
*   **Scene Understanding (`SceneUnderstandingModule.cpp`)**:
    *   **Goal**: Provide context about the environment (e.g., "Bedroom", "Crosswalk", "Train Station").
    *   **Algorithm**: ResNet18 trained on Places365 dataset.
    *   **Logic**: Classifies the entire frame into one of 365 scene categories. Helps the user understand *where* they are.
*   **OCR / Text Recognition (`OCRModule.cpp`)**:
    *   **Goal**: Read signage and labels.
    *   **Algorithm**: DBNet (Detection) + CRNN (Recognition).
    *   **Logic**: Detects text regions and converts them to speech. Useful for reading room numbers, street signs, or product labels.
*   **Path Guidance (`PathGuidanceModule.cpp`)**:
    *   **Goal**: Keep the user centered on the path.
    *   **Logic**: analyzes free space distribution. If the majority of free space is to the left, it suggests "Turn Left".

### 7. Audio Feedback Loop
The system uses a "Silence by Default" philosophy, alerting only on positive detection.
*   **CRITICAL**: High-pitch beep (2000Hz).
*   **WARNING**: Mid-pitch beep (1000Hz).
*   **INFO**: Low-pitch beep (Startup/Status).

### 8. Performance & Optimization
*   **Graph Pipeline**: Modules are executed in a dependency-aware order (Topological Sort).
*   **Async Processing**:
    *   **Depth**: Monocular Depth (MiDaS) runs in a background `std::future`, updating the depth map asynchronously (5 FPS) while the main loop runs at 30 FPS.
    *   **Audio**: Alerts are spawned in detached `std::thread`s to prevent UI blocking.
*   **Velocity Prediction**: `TemporalModule` uses linear velocity estimation to interpolate object positions during skipped frames, providing fluid visual feedback (30 FPS visual smoothness from 5 FPS detection).
*   **Conditional Pruning**: The **Execution Gates** system skips heavy modules based on the confidence of lighter upstream modules (e.g., Geometry).


---

## 🛠️ Build & Dependency System

We moved away from manual linking to a modern, reproducible pipeline using **vcpkg** and **CMake**.

*   **Package Manager**: `vcpkg` (Manifest Mode). No manual download of OpenCV required.
*   **Build System**: CMake (v3.14+). Links dependencies automatically.
*   **Automation**: Scripts for setup (`setup_vcpkg.bat`), model download (`download_models.bat`), and building (`build.bat`).

---

## 📂 Project Structure

```text
AVISION/
├── src/
│   ├── core/                   # --- THE BRAIN (Portable C++) ---
│   │   ├── geometry/           # Geometry, FreeSpace, EdgeSafety modules
│   │   ├── semantics/          # ObjectEngine, TemporalModule
│   │   ├── distance/           # Distance estimation algorithms
│   │   ├── depth/              # Depth estimation (MiDaS)
│   │   └── Engine.cpp          # Main safety loop
│   ├── platform/               # --- THE BODY (OS Specific) ---
│   │   ├── interfaces/         # Contracts (ICamera.h, IAudio.h)
│   │   └── desktop/            # Implementations for PC
│   └── main.cpp                # App Entry Point
├── vcpkg.json                  # Dependency manifest
├── CMakeLists.txt              # Build configuration
└── README.md                   # This Manual
```

---

## 🌍 Platform Support Status

| Platform | Status | Implementation |
| :--- | :--- | :--- |
| **Windows** | ✅ **Verified** | `src/platform/desktop` (OpenCV + WinAPI) |
| **Linux** | 🟢 **Compatible** | `src/platform/desktop` (OpenCV + ALSA/Console) |
| **Android** | 🟡 **Ready** | `src/platform/android` (JNI Bridge Created) - [Setup Guide](ANDROID_SETUP.md) |
| **iOS** | 🟡 **Ready** | `src/platform/ios` (Obj-C++ Bridge Created) |
| **Embedded** | 🟡 **Ready** | `src/platform/embedded` (Headless Service) |

### 🛠️ Embedded Build (Linux/Pi)
To build the headless runner for Raspberry Pi or Linux servers:
```bash
mkdir build && cd build
cmake .. -DPLATFORM=EMBEDDED
make
./AVisionHeadless
```

---

## 🔮 Roadmap

*   **Phase 1 (Done)**: Portable Core Architecture, Ground/Obstacle Detection, Audio Alerts.
*   **Phase 2 (Done)**: Semantic Understanding (Generic Engine: YOLOv8 / SSD).
*   **Phase 3 (Done)**: Platform Portability (Android JNI Bridge & Passive Core Refactor).
*   **Phase 4 (Done)**: Video Analysis CLI & Optimization.
*   **Phase 5 (Done)**: Depth Estimation Integration (MiDaS) & Safety Fusion.
*   **Phase 6 (Done)**: Advanced Safety Modules (Temporal Stabilization, Free Space Navigation, Drop-off Detection).
*   **Phase 7 (Done)**: Semantic Intelligence (Scene Understanding, OCR, Path Guidance).
*   **Phase 8 (Done)**: Pipeline Graph Architecture (DAG), Async Inference, & Velocity Prediction.
*   **Phase 9 (Done)**: Unified Metrics Layer, Confidence Scores, & Execution Gates.
*   **Phase 10 (Done)**: AI-Powered Image Tools (MobileSAM + Big-LaMa Generative Fill).

---

> **Philosophy**: "The system must never surprise the user." Safety > Features.
