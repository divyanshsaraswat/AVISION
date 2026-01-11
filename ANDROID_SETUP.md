# Android Port Setup Guide 📱

This guide explains how to take the **Portable C++ Core** we built and run it on an Android phone.

## Prerequisites
*   **Android Studio** (Latest Version)
*   **NDK (Side-by-side)** installed via SDK Manager.

## Step 1: Create Android Project
1.  Open Android Studio.
2.  **New Project** -> Select **"Native C++"** template.
3.  Name: `AVisionApp`
4.  Package Name: `com.avision.app` (Important: Must match JNI functions in `VisionBridge.cpp`).
5.  Language: **Kotlin**.
6.  Build Configuration Language: **Groovy DSL** (or Kotlin DSL).
7.  C++ Standard: **C++17**.

## Step 2: Import C++ Core
1.  Navigate to your new Android project folder: `app/src/main/cpp`.
2.  **Copy** the entire `AVISION/src/core` folder from this repo into `app/src/main/cpp/core`.
3.  **Copy** `AVISION/src/platform/android/VisionBridge.cpp` into `app/src/main/cpp`.

Structure should look like:
```
app/src/main/cpp/
├── native-lib.cpp (Delete this default file)
├── VisionBridge.cpp
└── core/
    ├── Engine.h
    ├── Engine.cpp
    ├── geometry/
    ├── distance/
    └── semantics/
```

## Step 3: Configure CMake (Android)
Update `app/src/main/cpp/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.22.1)
project("avision-mobile")

# Import OpenCV Android SDK (Download from OpenCV.org)
# (You must download OpenCV Android SDK and extract it first)
set(OpenCV_DIR "C:/path/to/opencv-android-sdk/sdk/native/jni") 
find_package(OpenCV REQUIRED)

add_library(avision-native SHARED
    VisionBridge.cpp
    core/Engine.cpp
    core/geometry/GeometryEngine.cpp
    core/distance/DistanceEngine.cpp
    core/semantics/ObjectEngine.cpp
)

target_link_libraries(avision-native
    ${OpenCV_LIBS}
    log
    android
)
```

## Step 4: Kotlin Integration (MainActivity.kt)
In your `MainActivity.kt`:

```kotlin
class MainActivity : AppCompatActivity(), CameraBridgeViewBase.CvCameraViewListener2 {
    
    // Load Library
    companion object {
        init { System.loadLibrary("avision-native") }
    }

    // JNI Functions
    external fun init(modelPath: String): Boolean
    external fun processFrame(matAddrInput: Long, matAddrDebug: Long): Boolean

    // ... Implementation of OpenCV Camera Listener ...
    override fun onCameraFrame(inputFrame: CvCameraViewFrame): Mat {
        val rgba = inputFrame.rgba()
        
        // Call C++ Core
        processFrame(rgba.nativeObjAddr, rgba.nativeObjAddr)
        
        return rgba
    }
}
```

## Summary
You have now bridged the **Exact Same C++ Logic** from `AVision.exe` to an Android App.
