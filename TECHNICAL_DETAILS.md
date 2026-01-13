# A-Vision Technical Reference ("The Minute Details")

This document serves as the "Code Bible" for the A-Vision project. It details the exact algorithms, heuristics, magic numbers, and implementation strategies used in the Core engine.

## 1. Object Engine (`src/core/semantics/ObjectEngine.cpp`)

The "Semantic Eye" of the system. It wraps OpenCV DNN to provide generic object detection.

### 1.1 Model Support
*   **Architecture**: Factory Pattern based on `ModelType` enum (`SSD_MOBILENET`, `YOLO_V8`).
*   **Format Detection**:
    *   `.caffemodel` -> Uses `cv::dnn::readNetFromCaffe`.
    *   `.onnx`, `.tflite` -> Uses `cv::dnn::readNet`.
*   **Backend**: 
    *   `DNN_BACKEND_OPENCV` (Standard C++ implementation).
    *   `DNN_TARGET_CPU` (Forced execution on CPU for edge compatibility).

### 1.2 Configuration (`selected_model.json`)
The system loads a JSON config at runtime.
*   **Input Resolution**: 
    *   SSD: Typically `300x300`.
    *   YOLO: Typically `640x640` (but Nano v8 uses `640` by default).
*   **Normalization**:
    *   **SSD**: `Mean = [127.5, 127.5, 127.5]`, `Scale = 1.0/127.5 (approx 0.007843)`.
    *   **YOLO**: `Mean = [0, 0, 0]`, `Scale = 1.0/255.0 (approx 0.003921)`, `SwapRB = True`.

### 1.3 Dataset & Labels
*   **Mechanism**: Switch based on `config.dataset` string.
*   **VOC (Legacy)**: 20 Classes.
    *   Indices: 0=background (skipped), 15=person.
*   **COCO (Modern)**: 80 Classes.
    *   Indices: 0=person, 2=car, 67=cell phone.
    *   **Note**: The system does NOT read labels from the model file; it uses hardcoded std::vectors in C++ for reliability.

### 1.4 Post-Processing Logic
*   **SSD**:
    *   Output Shape: `[1, 1, N, 7]` -> `(batch, class, score, x, y, w, h)`.
    *   Logic: Iterate rows, check `confidence > scoreThreshold`.
*   **YOLOv8**:
    *   Output Shape: `[1, 84, 8400]` (84 = 4 box coords + 80 class scores).
    *   **Step 1**: Transpose matrix to `[8400, 84]` for row-major iteration.
    *   **Step 2**: For each row, find `max(class_scores)`.
    *   **Step 3**: `NMSBoxes` (Non-Maximum Suppression) with `nmsThreshold=0.45` to remove duplicate boxes.

---

## 2. Geometry Engine (`src/core/geometry/GeometryEngine.cpp`)

The "Safety Layer". Uses classical Computer Vision (no AI) for determinism and speed.

### 2.1 Ground Detection (Safe Zone)
*   **Goal**: Determine if the floor immediately in front of the user is walkable.
*   **Region of Interest (ROI)**:
    *   `x = width/4`, `y = height - height/3`.
    *   `w = width/2`, `h = height/3`.
    *   (Essentially the bottom-center rectangle).
*   **Algorithm**: Canny Edge Detection.
    *   `threshold1 = 50`, `threshold2 = 150`.
*   **Heuristic**: "Texture Density".
    *   Count non-zero pixels in the edge map.
    *   **Threshold**: `0.05` (5%).
    *   **Logic**: If edge pixels > 5% of total ROI pixels -> **UNSAFE** (Red Box).
    *   *Why?* Flat floor has effectively 0% edges. Rubble, stairs, or crosswalk lines have >5%.

### 2.2 Obstacle Detection
*   **Goal**: Find physical objects blocking the path.
*   **Preprocessing**: 
    *   `Grayscale` -> `GaussianBlur (5x5)` -> `Inverse Binary Threshold (100, 255)`.
    *   This highlights dark objects against light backgrounds (or vice-versa depending on lighting).
*   **Filtering**:
    *   `cv::findContours`.
    *   **Area Filter**: Ignore contours `< 1000` pixels (Noise).
    *   **Height Filter**: Ignore contours in the top half of the screen (`y + h < rows/2`).
*   **Output**: Bounding Box + Relative Distance.

---

## 3. Distance Engine (`src/core/distance/DistanceEngine.h`)

Calculates "Time-to-Collision" based on 2D position.

### 3.1 Monocular Depth Estimation
*   **Technique**: "Horizon Line Assumption".
*   **Formula**: `RelativeDistance = (BoxBottomY / FrameHeight)`.
*   **Range**: `0.0` (Top of screen/Far) to `1.0` (Bottom of screen/User's feet).

### 3.2 Categorization Thresholds
*   **IMMEDIATE (Danger)**: `> 0.85` (Bottom 15% of screen).
    *   Triggers: High-pitch Audio / "CRITICAL STOP" text.
*   **NEAR (Warning)**: `> 0.60` (Bottom 40% of screen).
    *   Triggers: Mid-pitch Audio / "WARNING" text.
*   **FAR**: `< 0.60`.
    *   Triggers: Passive tracking only.

---

## 4. Video CLI (`src/video_cli.cpp`)

A specialized tool for testing and verification.

### 4.1 Comparison to Main App
*   **Main App**: Runs infinite loop reading from Camera (`0`). Plays system audio.
*   **CLI**: Runs loop reading from File (`path`). **NO Audio**.
*   **Visualization**:
    *   Layer 1: Raw Frame.
    *   Layer 2: Geometry Debug (Safe Zone Rect + Obstacle Rects).
    *   Layer 3: Warning Text (cv::putText at 20,50).
    *   Layer 4: AI Object Boxes (Green).
*   **Output**: 
    *   `std::cout` stream with timestamps.
    *   `result.txt` file write.

### 4.2 Known Behaviors
*   **Crosswalks**: Will trigger "CRITICAL STOP" (Red Zone) due to high edge density (stripes > 5%). This is intended "Fail Safe" behavior.
