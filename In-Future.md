1. 🧵 Threading (Asynchronous Semantic Core)
Current Issue: Even though we throttle object detection (every 5 frames), the frame still freezes for ~30-50ms when it runs. This glitches the "Geometry Engine" (the safety layer). Goal: Move ObjectEngine::detect to a background thread (std::async).

Safety Layer (Main Thread): Runs at 30+ FPS, strictly checking ground/obstacles.
Semantic Layer (Bg Thread): Updates overlay whenever latest results arrive.

2. 🗣️ Text-To-Speech (TTS)
Current Issue: We detect "Chair" but only print it to the console. Goal: Integrate a cross-platform TTS library (e.g., flite or platform native APIs via the bridge) so the user hears "Chair ahead".


3. ⏱️ Profiling
Goal: Implement a strictly measured "Latency Budget" monitor. If processing exceeds 33ms, automatically downgrade resolution or disable features to ensure safety.

Recommendation: Start with Threading. It yields the biggest safety improvement by unblocking the obstacle detector. Shall I begin refactoring Engine for async detection?