# 🚀 A-Vision v1.5: The Modular Perception Update

This release marks a major architectural evolution for A-Vision, transitioning from a monolithic system to a fully **Modular Core Engine**. v1.5 empowers developers and users to dynamically configure their assistive vision stack, enabling or disabling advanced perception features on the fly.

#### **🔥 Key Highlights**
*   **🧩 Modular Architecture**: New plug-and-play `IModule` system. Easily swap or toggle features like Depth, OCR, or Scene Understanding without recompiling the core logic.
*   **🛠️ Interactive Build System**: A brand new **TUI (Terminal User Interface)** Configuration Tool (`AVisionConfig`). It handles dependency checks, automatically downloads the correct ONNX models, and compiles the project with a single keypress (F1).
*   **👁️ New Perception Capabilities**:
    *   **Scene Understanding**: Context awareness (e.g., "Bedroom", "Crosswalk") powered by *ResNet18-Places365*.
    *   **OCR (Text Detection)**: Real-time text location using *DBNet* (now with custom model support).
    *   **Depth Estimation**: Monocular depth maps via *MiDaS* for enhanced obstacle avoidance.
*   **💻 AVision CLI**: A powerful command-line companion (`AVisionCLI`) for batch image processing, semantic segmentation, and AI in-painting tasks.

#### **✨ Improvements & Fixes**
*   **Visuals**: Unified "Cyber-Vision" branding and improved debug overlays.
*   **Stability**: Fixed webcam frame capture logic and resolved module initialization crashes.
*   **Usability**: Smart model recovery in the build system (auto-downloads missing files).
