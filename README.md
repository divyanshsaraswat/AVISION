# A-Vision (Assistive Vision System)

![Version](https://img.shields.io/badge/version-v0.5.0-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20Android%20%7C%20iOS-lightgrey)


<img width="2304" height="1856" alt="Gemini_Generated_Image_u2xzr3u2xzr3u2xz" src="https://github.com/user-attachments/assets/839f25f2-71ff-4d0a-83b8-d9ddcb70f359" />


**A-Vision** is a high-performance, cross-platform C++ engine designed to assist visually impaired users. Built with **Modern C++17**, **CMake**, and **OpenCV**, designed to run on low-power edge devices.

---

## 🧠 System Architecture: "Portable Core"

This project follows a strict **Systems Engineering** approach to ensure safety and portability. The code is divided into two distinct layers:

### 1. The Core ("The Brain")
*   **Location**: `src/core/`
*   **Dependencies**: Pure C++ STL, OpenCV (Core/ImgProc). **NO OS dependencies.**
*   **Responsibility**:
    *   **Geometry Engine**: Analyses the frame to find walkable ground and obstacles.
    *   **Distance Engine**: Estimates "Time-to-Collision" using monocular cues.
    *   **Risk Engine**: Decides *if* and *when* to alert the user (Safety Logic).
*   **Portability**: Compiles unchanged on Windows, Linux, Android (NDK), iOS, and Raspberry Pi.

### 2. The Platform ("The Body")
*   **Location**: `src/platform/`
*   **Dependencies**: OS-specific APIs (Windows SDK, Android CameraX, ALSA, etc.).
*   **Responsibility**:
    *   Captures frames and feeds them to the Core.
    *   Plays audio alerts triggered by the Core.
*   **Current Implementation**: `desktop/` (Windows/Linux compatible via OpenCV/Console).

---

## 🚀 Getting Started

### Prerequisites
*   **Git**: Version control.
*   **CMake**: Build system generator (v3.14+).
*   **C++ Compiler**: MSVC (Visual Studio 2022) on Windows, or GCC/Clang on Linux.

> **Note**: You do NOT need to manually install OpenCV. The project handles dependencies automatically via `vcpkg`.

### 🛠️ One-Click Setup (Windows)

1.  **Initialize Dependencies**:
    Run the setup script **once** to install the package manager.
    ```cmd
    .\setup_vcpkg.bat
    ```

2.  **Download AI Models (Phase 2)**:
    Required for Object Detection.
    .\download_models.bat
    ```
    *Note: This is now an interactive wizard! Follow the prompts to select your preferred model (YOLOv8, MobileNet, etc).*

3.  **Build the System**:
    This script configures CMake, downloads OpenCV (if needed), and compiles the Release build.
    ```cmd
    .\build.bat
    ```
    *First run may take 10-15 minutes to compile OpenCV. Subsequent builds are instant.*

3.  **Run the Application**:
    ```cmd
    build\Release\AVision.exe
    ```

### 🎥 Video Analysis (New)
You can now process video files directly using the new CLI tool:
```cmd
build\Release\AVisionCLI.exe --video "path/to/video.mp4"
```

### 🧹 Disk Space Management
Building from source creates large intermediate files (~8GB). To reclaim space after a successful build:
```cmd
.\clean_vcpkg.bat
```
*This keeps the libraries but removes the source code and build objects.*

---

## 📂 Project Structure

```text
AVISION/
├── src/
│   ├── core/                   # --- THE BRAIN ---
│   │   ├── geometry/           # Ground & Obstacle detection logic
│   │   ├── distance/           # Distance estimation algorithms
│   │   └── Engine.cpp          # Main safety loop (The "OS" of the system)
│   ├── platform/               # --- THE BODY ---
│   │   ├── interfaces/         # Contracts (ICamera.h, IAudio.h)
│   │   └── desktop/            # Implementations for PC
│   └── main.cpp                # App Entry Point
├── vcpkg.json                  # Dependency manifest (like package.json)
├── CMakeLists.txt              # Build configuration
└── README.md                   # This Manual
```

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
(Note: You may need to modify `CMakeLists.txt` to enable the `HeadlessRunner` target based on the flag).

## 🔮 Roadmap

*   **Phase 1 (Done)**: Portable Core Architecture, Ground/Obstacle Detection, Audio Alerts.
*   **Phase 2 (Done)**: Semantic Understanding (Generic Engine: YOLOv8 / SSD).
*   **Phase 3 (Done)**: Platform Portability (Android JNI Bridge & Passive Core Refactor).
*   **Phase 4 (In Progress)**: Video Analysis CLI & Optimization.

---

> **Philosophy**: "The system must never surprise the user." Safety > Features.
