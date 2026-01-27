#include "ImageCLI.h"
#include <iostream>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <sstream>
#include "core/segmentation/SegmentationModule.h"

// Helper to check if file exists
static bool fileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

// Helper to parse bbox x,y,w,h
static cv::Rect parseBBox(const std::string& s) {
    std::vector<int> vals;
    std::stringstream ss(s);
    std::string segment;
    while(std::getline(ss, segment, ',')) {
        vals.push_back(std::stoi(segment));
    }
    if (vals.size() != 4) return cv::Rect();
    return cv::Rect(vals[0], vals[1], vals[2], vals[3]);
}

void ImageCLI::printHelp() {
    std::cout << "avision image\n\n"
              << "Usage:\n"
              << "  avision image segment [options]\n"
              << "  avision image fill [options]\n\n"
              << "Commands:\n"
              << "  segment    Segment an object from an image using ROI-based deterministic segmentation\n"
              << "  fill       Fill or remove segmented regions using classical inpainting (CPU-safe)\n\n"
              << "Generative fill is intentionally not supported in v1.5\n";
}

int ImageCLI::run(int argc, char** argv) {
    if (argc < 3) {
        printHelp();
        return 1;
    }

    std::cout << "[DEBUG] argc: " << argc << "\n";
    for(int k=0; k<argc; ++k) std::cout << "argv[" << k << "]: " << argv[k] << "\n";

    std::string cmd = argv[2];
    if (cmd == "segment") {
        return handleSegment(argc, argv);
    } else if (cmd == "fill") {
        return handleFill(argc, argv);
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        printHelp();
        return 1;
    }
}

int ImageCLI::handleSegment(int argc, char** argv) {
    std::string inputPath, outputPath, bboxStr, seedStr;
    bool interactive = false;
    bool overwrite = false;
    cv::Rect roi; // For floodfill bounding rect output
    cv::Rect rect; 
    cv::Point seedPt(-1, -1);
    int threshold = 20; 
    cv::Mat binMask; 


    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input" && i+1 < argc) inputPath = argv[++i];
        else if (arg == "--output" && i+1 < argc) outputPath = argv[++i];
        else if (arg == "--bbox" && i+1 < argc) bboxStr = argv[++i];
        else if (arg == "--seed" && i+1 < argc) {
             seedStr = argv[++i];
             std::cout << "MATCHED SEED! Val='" << seedStr << "'\n";
        }
        else if (arg == "--interactive") interactive = true;
        else if (arg == "--overwrite") overwrite = true;
    }

    // Validation moved to later stages
    // if (inputPath.empty() || outputPath.empty()) ...

    if (!overwrite && fileExists(outputPath)) {
        std::cerr << "Error: Output file exists (use --overwrite)\n";
        return 1;
    }

    // Resolve Input Path
    namespace fs = std::filesystem;
    fs::path inPath(inputPath);
    if (!fs::exists(inPath)) {
        // Try relative to Executable (build/Release)
        fs::path exeDir = fs::path(argv[0]).parent_path();
        fs::path tryPath = exeDir / inputPath;
        if (fs::exists(tryPath)) {
            inPath = tryPath;
            std::cout << "[Info] Found image at: " << inPath.string() << "\n";
        }
    }

    cv::Mat img = cv::imread(inPath.string());
    if (img.empty()) {
        std::cerr << "Error: Could not read valid input image\n";
        return 1;
    }

    cv::Point interactiveSeed(-1, -1);
    
    if (interactive) {
        std::cout << "--- Interactive Mode ---\n";
        std::cout << "  Left Click      : Add Foreground Point (Green)\n";
        std::cout << "  Right Click     : Add Background Point (Red)\n";
        std::cout << "  Drag (Left Btn) : Draw Bounding Box (Blue)\n";
        std::cout << "  SPACE / ENTER   : Run Segmentation\n";
        std::cout << "  'c'             : Clear All Prompts\n";
        std::cout << "  's'             : Save Result & Exit\n";
        std::cout << "  'q' / ESC       : Quit without saving\n";
        std::cout << "------------------------\n";

        // State
        std::vector<cv::Point> points;
        std::vector<int> labels; // 1=FG, 0=BG
        cv::Rect box;
        cv::Mat currentVisual = img.clone();
        
        // Context for Callback
        struct InteractionContext {
            cv::Mat refImg;
            std::vector<cv::Point>* points;
            std::vector<int>* labels;
            cv::Rect* box;
            bool isDragging = false;
            cv::Point dragStart;
            bool dirty = true;
            cv::Mat currentMask; // Last result
        } ctx;
        
        ctx.refImg = img; // Shared reference? Actually just need const access.
        ctx.points = &points;
        ctx.labels = &labels;
        ctx.box = &box;
        
        std::string winName = "Interactive Segmentation";
        cv::namedWindow(winName);

        cv::setMouseCallback(winName, [](int event, int x, int y, int flags, void* userdata) {
            auto* c = (InteractionContext*)userdata;
            
            if (event == cv::EVENT_LBUTTONDOWN) {
                c->isDragging = true;
                c->dragStart = cv::Point(x, y);
            } 
            else if (event == cv::EVENT_MOUSEMOVE) {
                if (c->isDragging) {
                    // Update box during drag
                    int x1 = std::min(c->dragStart.x, x);
                    int y1 = std::min(c->dragStart.y, y);
                    int w = std::abs(x - c->dragStart.x);
                    int h = std::abs(y - c->dragStart.y);
                    *c->box = cv::Rect(x1, y1, w, h);
                    c->dirty = true;
                }
            } 
            else if (event == cv::EVENT_LBUTTONUP) {
                if (c->isDragging) {
                    c->isDragging = false;
                    int dx = x - c->dragStart.x;
                    int dy = y - c->dragStart.y;
                    
                    // Threshold to distinguish click vs drag (e.g. 5 pixels)
                    if (std::abs(dx) < 5 && std::abs(dy) < 5) {
                        // It's a Click -> Add FG Point
                        c->points->push_back(cv::Point(x, y));
                        c->labels->push_back(1);
                        
                        // Don't modify box if it was just a click
                        // (Unless box was being drawn? No, simple logic)
                        // If we had a previous box, we keep it. 
                        // But the drag update set it to 0-size? No, we check dist.
                        // Actually, MOUSEMOVE updated it to tiny rect. We should check bounds.
                        if (c->box->width < 5 && c->box->height < 5) {
                             // Revert to valid box or empty if invalid
                             if (c->box->width < 5) *c->box = cv::Rect(); // Reset if too small
                        }
                    } else {
                        // Keep the dragged box
                    }
                    c->dirty = true;
                }
            } 
            else if (event == cv::EVENT_RBUTTONDOWN) {
                // Background point
                c->points->push_back(cv::Point(x, y));
                c->labels->push_back(0); 
                c->dirty = true;
            }
        }, &ctx);

        avision::SegmentationModule segModule;

        while(true) {
            if (ctx.dirty) {
                // Redraw Base
                currentVisual = img.clone();
                
                // Draw Box
                if (box.width > 0 && box.height > 0) {
                    cv::rectangle(currentVisual, box, cv::Scalar(255, 0, 0), 2);
                }
                
                // Draw Points
                for(size_t i=0; i<points.size(); ++i) {
                    cv::Scalar color = (labels[i] == 1) ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255);
                    cv::circle(currentVisual, points[i], 4, color, -1);
                    // Add outline for visibility
                    cv::circle(currentVisual, points[i], 5, cv::Scalar(0,0,0), 1);
                }
                
                // Draw Mask Overlay
                if (!ctx.currentMask.empty()) {
                   cv::Mat redOverlay = cv::Mat::zeros(img.size(), CV_8UC3);
                   redOverlay.setTo(cv::Scalar(0, 0, 255), ctx.currentMask); // Red
                   cv::addWeighted(currentVisual, 1.0, redOverlay, 0.4, 0, currentVisual);
                }
                
                cv::imshow(winName, currentVisual);
                ctx.dirty = false;
            }
            
            int key = cv::waitKey(30);
            if (key == 'q' || key == 27) { // Quit
                 return 0;
            }
            if (key == 's') { // Save
                 binMask = ctx.currentMask;
                 break; 
            }
            if (key == 'c') { // Clear
                 points.clear();
                 labels.clear();
                 box = cv::Rect();
                 ctx.currentMask = cv::Mat();
                 ctx.dirty = true;
                 std::cout << "[Interactive] Cleared.\n";
            }
            if (key == 32 || key == 13) { // Run
                 std::cout << "[Interactive] Segmenting...\n";
                 ctx.currentMask = segModule.segment(img, points, labels, box);
                 ctx.dirty = true;
                 
                 std::cout << "Done. Mask Non-Zero: " << cv::countNonZero(ctx.currentMask) << "\n";
            }
        }
        
        cv::destroyWindow(winName);
        
        if (binMask.empty()) {
            std::cerr << "Warning: Exiting without a valid mask selected.\n";
            return 0;
        }

    } else if (!bboxStr.empty()) {
        roi = parseBBox(bboxStr);
    } else if (seedStr.empty()) {
        std::cerr << "Error: Must specify --bbox, --interactive, or --seed\n";
        return 1;
    }

    // Seed Parsing Logic (moved/kept)
    if (!seedStr.empty()) {
        // ... (Parsing logic from original file needs to be preserved or ensures it runs)
        // Wait, the replacement block above replaced the seed parsing logic?
        // The original file had seed parsing AFTER the check.
        // My replacement cut it off?
        // I need to check if I deleted the seed parsing logic.
        // The replacement was around line 110 to 175.
        // The seed parsing was at line 183.
        // My replacement ended at line 175 (relative to original file).
        // So seed parsing should still be there.
    }

    if (!interactive) {
         avision::SegmentationModule segModule;
         binMask = segModule.segment(img, roi, seedPt);
    }

    // Seed Parsing
    if (!seedStr.empty()) {
        std::cout << "Parsing seed: " << seedStr << std::endl;
        std::vector<int> vals;
        std::stringstream ss(seedStr);
        std::string segment;
        while(std::getline(ss, segment, ',')) {
            try {
                vals.push_back(std::stoi(segment));
            } catch(...) {
                std::cerr << "stoi failed for: " << segment << std::endl;
            }
        }
        
        std::cout << "Parsed " << vals.size() << " values." << std::endl;
        
        if (vals.size() >= 2) {
             seedPt = cv::Point(vals[0], vals[1]);
             std::cout << "SeedPt set to: " << seedPt << std::endl;
        }
        if (vals.size() >= 3) threshold = vals[2];
    }

    if (inputPath.empty()) {
        std::cerr << "Usage: avision image segment --input <img.jpg> (--bbox x,y,w,h | --seed x,y[,thresh] | --interactive) [--output <mask.png>]\n";
        return 1;
    }

    if (outputPath.empty()) {
        outputPath = "output.png";
    }



    // --- SEGMENTATION MODULE INTEGRATION ---
    if (!interactive) {
        avision::SegmentationModule segModule;
        binMask = segModule.segment(img, roi, seedPt);
    }

    if (binMask.empty()) {
        std::cerr << "Error: Segmentation failed (backend returned empty mask).\n";
        return 1;
    }

    std::cout << "[Segment] Mask Size: " << binMask.size() << ", Non-Zero: " << cv::countNonZero(binMask) << "\n";

    // Apply Mask Overlay (Red Overlay on Input Image)
    cv::Mat result = img.clone();
    cv::Mat overlay;
    result.copyTo(overlay);
    
    // Create red mask: B=0, G=0, R=255
    for(int y=0; y<binMask.rows; ++y) {
        for(int x=0; x<binMask.cols; ++x) {
            if(binMask.at<uchar>(y,x) > 0) {
                // Set red channel to 255, blend with original
                cv::Vec3b& pixel = overlay.at<cv::Vec3b>(y,x);
                pixel[2] = 255; // Red
            }
        }
    }
    
    // Blend: 0.7 * Original + 0.3 * RedMask
    cv::addWeighted(overlay, 0.4, result, 0.6, 0.0, result);

    // Save Output
    if (cv::imwrite(outputPath, result)) {
        std::cout << "Saved segmented image (overlay) to: " << outputPath << "\n";
        
        // Also save the binary mask for 'fill' command usage
        std::string maskPath = outputPath;
        size_t lastDot = maskPath.find_last_of(".");
        if (lastDot != std::string::npos) {
            maskPath.insert(lastDot, "_mask");
        } else {
            maskPath += "_mask.png";
        }
        
        if (cv::imwrite(maskPath, binMask)) {
             std::cout << "Saved binary mask to: " << maskPath << "\n";
        }
    } else {
        std::cerr << "Error: Failed to save to " << outputPath << "\n";
        return 1;
    }

    return 0;
}

#include "core/filling/FillModule.h"

// ... (previous includes)

int ImageCLI::handleFill(int argc, char** argv) {
    std::string inputPath, maskPath, outputPath;
    std::string mode = "smart"; // default
    bool overwrite = false;
    int padding = 50;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input" && i+1 < argc) inputPath = argv[++i];
        else if (arg == "--mask" && i+1 < argc) maskPath = argv[++i];
        else if (arg == "--output" && i+1 < argc) outputPath = argv[++i];
        else if (arg == "--padding" && i+1 < argc) padding = std::stoi(argv[++i]);
        else if (arg == "--overwrite") overwrite = true;
    }

    if (inputPath.empty() || maskPath.empty() || outputPath.empty()) {
         std::cerr << "Usage: avision image fill --input <img.jpg> --mask <mask.png> --output <filled.jpg> [--padding 50]\n";
         return 1;
    }
    
    if (!overwrite && fileExists(outputPath)) {
        std::cerr << "Error: Output file exists (use --overwrite)\n";
        return 1;
    }

    // Smart Path Resolution (Copied from handleSegment)
    namespace fs = std::filesystem;
    fs::path inPath(inputPath);
    if (!fs::exists(inPath)) {
        fs::path exeDir = fs::path(argv[0]).parent_path();
        fs::path tryPath = exeDir / inputPath;
        if (fs::exists(tryPath)) { inPath = tryPath; }
    }
    
    // Check mask too
    fs::path mPath(maskPath);
    if (!fs::exists(mPath)) {
         fs::path exeDir = fs::path(argv[0]).parent_path();
         fs::path tryPath = exeDir / maskPath;
         if (fs::exists(tryPath)) { mPath = tryPath; }
    }

    cv::Mat img = cv::imread(inPath.string());
    cv::Mat mask = cv::imread(mPath.string(), cv::IMREAD_GRAYSCALE); // Read as simple mask

    if (img.empty() || mask.empty()) {
        std::cerr << "Error: Could not open input or mask.\n";
        return 1;
    }
    
    if (img.size() != mask.size()) {
        std::cerr << "Error: Image and Mask dimensions do not match.\n";
        return 1;
    }

    // --- FILL MODULE INTEGRATION ---
    avision::FillModule fillModule;
    cv::Mat result = fillModule.fill(img, mask, padding);

    if (cv::imwrite(outputPath, result)) {
        std::cout << "[Fill] Saved filled image to: " << outputPath << "\n";
        return 0;
    } else {
        std::cerr << "Error: Failed to write output\n";
        return 1;
    }
}
