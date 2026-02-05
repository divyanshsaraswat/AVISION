#include <fstream>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <nlohmann/json.hpp>
#include <functional>

// FTXUI
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

using json = nlohmann::json;
using namespace ftxui;

// --- Data Structures ---

struct ModuleConfig {
    std::string name;
    std::string friendlyName;
    bool enabled;
    std::map<std::string, std::string> params;
    
    // UI State
    int selectedModelIdx = 0;
    std::string customUrl = "";
    std::vector<std::string> availableModels; 
};

// Friendly Name Map (reused)
std::map<std::string, std::string> friendlyNames = {
    {"ObjectModule", "Detection"},
    {"DepthModule", "Depth Estimation"},
    {"OCRModule", "OCR"},
    {"SceneUnderstandingModule", "Scene Understanding"},
    {"SegmentationModule", "Semantic Segmentation"},
    {"PoseModule", "Pose Estimation"},
    {"FaceModule", "Facial Recognition"},
    {"TTSModule", "Text-to-Speech"},
    {"GeometryModule", "Geometry & Safety"},
    {"TemporalModule", "Temporal Smoothing"},
    {"FreeSpaceModule", "Free Space"},
    {"EdgeSafetyModule", "Edge Safety"},
    {"PathGuidanceModule", "Path Guidance"},
    {"FillModule", "Image In-Painting"}
};

// Global Config State
std::vector<ModuleConfig> modules;

std::map<std::string, std::vector<std::string>> modelOptions = {
    {"ObjectModule", {"MobileNetSSD", "YOLOv8n", "YOLOv8s", "Custom..."}},
    {"DepthModule", {"MiDaS Small", "MiDaS Large", "Custom..."}},
    {"OCRModule", {"Tesseract", "EasyOCR", "Custom..."}},
    {"SceneUnderstandingModule", {"ResNet18 Places", "ResNet50 Places", "Custom..."}},
    {"SegmentationModule", {"DeepLabv3", "MobileSAM", "Custom..."}},
    {"PoseModule", {"MoveNet", "OpenPose", "Custom..."}},
    {"FaceModule", {"InsightFace", "Custom..."}},
    {"TTSModule", {"FastSpeech2", "Tacotron2", "Custom..."}}
};

// Build State
std::atomic<bool> is_building(false);
std::atomic<float> build_progress(0.0f);
std::atomic<int> build_step_index(0);
std::string build_status_msg = "Idle";
std::mutex build_mutex;
std::function<void()> g_on_build_success;

// --- IO Helper ---

void LoadConfig(const std::string& path) {
    modules.clear();
    std::ifstream f(path);
    if (!f.is_open()) return;
    
    json j;
    f >> j;
    
    if (j.contains("modules")) {
        for (const auto& m : j["modules"]) {
            ModuleConfig mc;
            mc.name = m["name"];
            mc.friendlyName = friendlyNames.count(mc.name) ? friendlyNames[mc.name] : mc.name;
            mc.enabled = m["enabled"];
            if (m.contains("params")) {
                for (auto& el : m["params"].items()) {
                    mc.params[el.key()] = el.value().get<std::string>();
                }
            }
            mc.availableModels = {"Default"};
            if (modelOptions.count(mc.name)) {
                mc.availableModels = modelOptions[mc.name];
            }
            modules.push_back(mc);
        }
    }
}

void SaveConfig(const std::string& path) {
    json j;
    j["pipeline"]["nodes"] = json::array();
    json j_mods = json::array();
    for (const auto& mc : modules) {
        json m;
        m["name"] = mc.name;
        m["enabled"] = mc.enabled;
        json p;
        for (const auto& pair : mc.params) {
            p[pair.first] = pair.second;
        }
        m["params"] = p;
        j_mods.push_back(m);
        j["pipeline"]["nodes"].push_back(mc.name);
    }
    j["modules"] = j_mods;
    std::ofstream f(path);
    f << j.dump(4);
}

// --- Build Logic ---
void RunBuild() {
    std::lock_guard<std::mutex> lock(build_mutex);
    if (is_building) return; // Prevent double trigger
    is_building = true;
    build_progress = 0.0f;
    build_status_msg = "Initializing...";
    build_step_index = 0;
    
    // Launch build in background
    std::thread build_thread([] {
        // 1. Clear old log
        { std::ofstream("build_log.txt"); }

        // 2. Launch build (non-blocking)
        // We use "cmd /c" to run the batch file. We don't wait for it here directly if we want to monitor.
        // Actually, if we want to monitor safely, we can just run it synchronously in this thread
        // because this thread IS detached from the UI thread!
        // But the previous issue was system() blocking... wait.
        // The UI runs in 'main' thread. 'RunBuild' creates a std::thread.
        // So blocking inside this std::thread is perfectly fine! 
        // The previous 'blocking' issue might have been because 'RunBuild' didn't detach or I messed up.
        // But wait, the user said "it seems it's stuck" when I used system("call build.bat...").
        // "system" blocks the calling thread. If calling thread is UI thread, UI freezes.
        // But I was already using std::thread + detach.
        // Ah, maybe the previous implementation didn't detach correctly or I misread.
        // Let's try running system() properly in this thread, but redirected.
        
        // However, we need to read the log WHILE it is writing. 
        // So we need TWO threads? Or one thread spurts the process, another monitors?
        // Simpler: Start process with "start /b" or just specific command that runs in background,
        // OR better: use popen? No, sticking to file redirection is easier for Windows portability.
        
        // We will stick to "start /min" strategy but simpler monitoring loop.
        system("start \"\" /min run_build_core.bat");

        // 3. Monitor Log
        bool done = false;
        int timeout_sec = 600; // 10 min timeout
        auto start_time = std::chrono::steady_clock::now();

        while (!done) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            // Read Log
            std::ifstream log_file("build_log.txt");
            std::string content((std::istreambuf_iterator<char>(log_file)), std::istreambuf_iterator<char>());
            log_file.close();

            // Calculate state locally first to avoid UI flickering
            float next_progress = build_progress;
            std::string next_msg = build_status_msg;
            int next_step = build_step_index;

            // Heuristics for progress
            // We only upgrade progress, never downgrade (unless failed)
            if (content.find("[SETUP]") != std::string::npos) {
                if (next_progress < 0.1f) {
                    next_msg = "Checking Dependencies...";
                    next_progress = 0.1f;
                }
            }
            if (content.find("[BUILD] compiling") != std::string::npos) {
                if (next_progress < 0.3f) {
                     next_msg = "Compiling C++ Code...";
                     next_step = 1;
                     next_progress = 0.3f;
                }
            }
            // Rough estimation based on file size if compiling
            if (next_step == 1) {
               if (content.length() > 500 && next_progress < 0.4f) next_progress = 0.4f;
               if (content.length() > 2000 && next_progress < 0.5f) next_progress = 0.5f;
               if (content.length() > 5000 && next_progress < 0.6f) next_progress = 0.6f;
            }

            if (content.find(".exe") != std::string::npos && content.find("LINK") == std::string::npos) {
                 if (next_progress < 0.8f) {
                     next_msg = "Linking & Packaging...";
                     next_step = 2;
                     next_progress = 0.8f;
                 }
            }

            if (content.find("[SUCCESS] Build complete") != std::string::npos) {
                next_msg = "Build Success!";
                next_progress = 1.0f;
                next_step = 4;
                done = true;
                if (g_on_build_success) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    g_on_build_success();
                }
            }
            if (content.find("[ERROR]") != std::string::npos || content.find("Build failed") != std::string::npos) {
                next_msg = "Build Failed! (Check build_log.txt)";
                next_progress = 0.0f; 
                done = true;
            }

            // Timeout
             if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(timeout_sec)) {
                next_msg = "Timeout (Check Logs)";
                done = true;
             }
             
             // Commit updates atomically(ish)
             build_status_msg = next_msg;
             build_progress = next_progress;
             build_step_index = next_step;
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        is_building = false;
    });
    build_thread.detach();
}

// --- Main UI ---

int main(int argc, const char** argv) {
    try {
        auto screen = ScreenInteractive::Fullscreen();
        
        std::string configPath = "models/modules.json";
        if (std::ifstream(configPath).fail()) {
             if (std::ifstream("../../../models/modules.json").good()) configPath = "../../../models/modules.json";
             else if (std::ifstream("../../models/modules.json").good()) configPath = "../../models/modules.json";
        }
        LoadConfig(configPath);
        
        // System Settings vars
        int backend_selected = 1; // CUDA default
        std::vector<std::string> backends = {"CPU", "CUDA", "DirectML", "OpenVINO"};
        
        int device_selected = 1; // Jetson default
        std::vector<std::string> devices = {"Desktop", "Jetson Nano", "Raspberry Pi 4", "Android"};

        int memory_selected = 1;
        std::vector<std::string> memories = {"512 MB", "1024 MB", "2048 MB", "4096 MB"};

        int latency_selected = 1;
        std::vector<std::string> latencies = {"10 ms", "50 ms", "100 ms", "500 ms"};

        // --- Components ---

        // 1. Modules List (Left Panel)
        auto modules_container = Container::Vertical({});
        for (auto& mod : modules) {
            auto checkbox = Checkbox(mod.friendlyName, &mod.enabled); 
            auto dropdown = Dropdown(&mod.availableModels, &mod.selectedModelIdx);
            
            auto row = Container::Horizontal({
                checkbox,
                dropdown
            });
            
            auto row_renderer = Renderer(row, [checkbox, dropdown, name=mod.friendlyName] {
                // Text is White, but we can highlight focused row
                bool focused = checkbox->Focused() || dropdown->Focused(); 
                
                return hbox({
                    checkbox->Render() | size(WIDTH, GREATER_THAN, 32) | color(Color::White) | center, 
                    text(": ") | color(Color::BlueLight) | center,
                    dropdown->Render() | flex 
                }) | (focused ? bold : nothing);
            });
            modules_container->Add(row_renderer);
        }

        // 2. System Settings (Right Panel Bottom)
        auto backend_drop = Dropdown(&backends, &backend_selected);
        auto device_drop = Dropdown(&devices, &device_selected);
        auto memory_drop = Dropdown(&memories, &memory_selected);
        auto latency_drop = Dropdown(&latencies, &latency_selected);

        auto settings_list_container = Container::Vertical({
            Container::Horizontal({ Renderer([]{ return text("Backend      : ") | center; }), backend_drop }),
            Container::Horizontal({ Renderer([]{ return text("Target Device: ") | center; }), device_drop }),
            Container::Horizontal({ Renderer([]{ return text("Max Memory   : ") | center; }), memory_drop }),
            Container::Horizontal({ Renderer([]{ return text("Max Latency  : ") | center; }), latency_drop }),
        });
        
        auto settings_renderer = Renderer(settings_list_container, [settings_list_container] { 
            return window(text(" System Settings "), settings_list_container->Render() | color(Color::White)) 
                   | color(Color::Green);
        });

        // 3. Build Status (Reactive)
        auto build_status_renderer = Renderer([&] {
             float p = build_progress;
             int step = build_step_index;
             
             auto status_item = [&](std::string label, int idx) {
                 if (step > idx) return hbox({text(label), text(" Done") | color(Color::Green) | bold});
                 if (step == idx && is_building) return hbox({text(label), text(" In Progress") | color(Color::Yellow) | bold });
                 return hbox({text(label), text(" Pending") | color(Color::Red) });
             };

             return window(text(" Build Status "), 
                vbox({
                     text(build_status_msg) | bold | color(Color::White),
                     // Custom Pixelated Progress Bar
                     hbox([&] {
                         Elements bars;
                         int total_bars = 40;
                         int filled = (int)(p * total_bars);
                         for(int i=0; i<total_bars; ++i) {
                             if(i < filled) bars.push_back(text("▮") | color(Color::Orange1));
                             else bars.push_back(text("▮") | color(Color::GrayDark));
                         }
                         bars.push_back(text(" " + std::to_string((int)(p * 100)) + "%") | color(Color::White) | bold);
                         return bars;
                     }()),
                     text(" "),
                     status_item("Compiling Modules... ", 1),
                     status_item("Optimizing Binaries... ", 2),
                     status_item("Packaging Files... ", 3),
                })
             ) | color(Color::Magenta);
        });

        // 4. Footer & Actions
        auto RenderAction = [&](Component btn, std::string key, std::string label) {
            return Renderer(btn, [btn, key, label] {
                bool focused = btn->Focused();
                return hbox({
                    text("[ "),
                    text(key) | bold | color(Color::White),
                    text(" ] "),
                    text(label) | bold | color(Color::White)
                }) | (focused ? inverted : nothing);
            });
        };

        // Actions
        auto on_quit = screen.ExitLoopClosure();
        g_on_build_success = on_quit;

        auto on_save = [&] { 
            SaveConfig(configPath); 
            std::thread t(RunBuild);
            t.detach(); 
        };

        auto btn_save_logic = Button("Save & Build", on_save, ButtonOption::Simple());
        auto btn_save = RenderAction(btn_save_logic, "F1", "Save & Build");

        auto btn_quit_logic = Button("Exit", on_quit, ButtonOption::Simple());
        auto btn_quit = RenderAction(btn_quit_logic, "F2", "Exit");
        
        // --- Structural Logic (Container Hierarchy) ---
        // Left Column: Modules + Guide
        auto modules_with_guide = Renderer(modules_container, [modules_container] {
             return vbox({
                 modules_container->Render() | flex,
                 separator() | color(Color::Blue),
                 text(" [ Space ] Toggle    [ Enter ] Select Model ") | color(Color::White) | hcenter
             });
        });
        auto left_panel = modules_with_guide; 
        
        // Right Column: Status + Settings
        auto right_panel = Container::Vertical({
            build_status_renderer,
            settings_renderer
        });

        auto main_container = Container::Horizontal({
            left_panel,
            right_panel
        });

        // --- Visual Rendering (The Aesthetics) ---
        auto main_renderer = Renderer(main_container, [&] {
             return vbox({
                 // Header
                 hbox({
                     vbox({
                         text(R"(⠀⠀⠀⠀⢀⣴⠶⣶⡄⠀⠀⠀⠀)") | color(Color::Green),
                         text(R"(⢀⣴⣧⠀⠸⣿⣀⣸⡇⠀⢨⡦⣄)") | color(Color::Cyan),
                         text(R"(⠘⣿⣿⣄⠀⠈⠛⠉⠀⣠⣾⡿⠋)") | color(Color::Cyan),
                         text(R"(⠀⠀⠈⠛⠿⠶⣶⡶⠿⠟⠉⠀⠀)") | color(Color::Cyan)
                     }) | center,
 
                     text("    "), // Gap
 
                     // Logo Text
                     vbox({
                         text(R"( _________________________________________________________________________________________________________ )") | color(Color::Cyan) | bold,
                         text(R"(    ___ _   __________________  _  __  _________  _  _________________  _____  ___ ______________  _  __)") | color(Color::Cyan) | bold,
                         text(R"(   / _ | | / /  _/ __/  _/ __ \/ |/ / / ___/ __ \/ |/ / __/  _/ ___/ / / / _ \/ _ /_  __/  _/ __ \/ |/ /)") | color(Color::Cyan) | bold,
                         text(R"(  / __ | |/ // /_\ \_/ // /_/ /    / / /__/ /_/ /    / _/_/ // (_ / /_/ / , _/ __ |/ / _/ // /_/ /    / )") | color(Color::Cyan) | bold,
                         text(R"( /_/ |_|___/___/___/___/\____/_/|_/  \___/\____/_/|_/_/ /___/\___/\____/_/|_/_/ |_/_/ /___/\____/_/|_/  )") | color(Color::Cyan) | bold,
                         text(R"( _________________________________________________________________________________________________________ )") | color(Color::Cyan) | bold
                     }) | center
                 }) | center | border | color(Color::Cyan),

                 // Body
                 hbox({
                     // Left Visuals
                     window(text(" Modules & Models "), 
                        left_panel->Render() | vscroll_indicator | frame | flex
                     ) | color(Color::Blue) | flex,

                     // Right Visuals
                     vbox({
                         build_status_renderer->Render() | flex,
                         settings_renderer->Render() | flex
                     }) | flex
                 }) | flex,

                 // Footer
                 window(text(" Actions "), hbox({
                     filler(),
                     btn_save->Render(),
                     text(" | ") | center,
                     btn_quit->Render()
                 }) | center) | color(Color::GrayDark)
             });
        });

        // Animation Loop
        std::thread animation([&] {
            while(true) {
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(100ms);
                if(is_building && build_progress < 0.8f) {
                     build_progress = build_progress + 0.01f;
                     if(build_progress > 0.3f && build_step_index < 2) build_step_index = 2;
                     if(build_progress > 0.6f && build_step_index < 3) build_step_index = 3;
                }
                screen.Post(Event::Custom);
            }
        });
        animation.detach();

        // Global Event Handler for F-Keys
        auto main_component = CatchEvent(main_renderer, [&](Event event) {
            if (event == Event::F1) { on_save(); return true; }
            if (event == Event::F2) { on_quit(); return true; }
            return false;
        });

        screen.Loop(main_component);

    } catch (const std::exception& e) {
        std::cerr << "ConfigTool Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
