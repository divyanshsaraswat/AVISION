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
        // Mode 1: Interactive BBox (default behavior)
        // Mode 2: Interactive Seed (if user passed --seed but no args? or generic interactive flag)
        // Simplification: If --seed flag is NOT passed, standard selectROI.
        // If user wants interactive Seed, we probably need a way to distinguish.
        // For now, let's allow 'interactive' to support CLICKING if not dragging.
        
        // Actually, user request: "give me interactive window to choose the point"
        // Let's implement a custom mouse callback.
        
        std::cout << "[Interactive] Click to select seed point (Magic Wand) OR Drag to select ROI (GrabCut).\n";
        std::cout << "Press SPACE/ENTER to confirm selection.\n";
        
        // Custom Mouse Callback Logic
        static cv::Point clickedPt(-1, -1);
        static cv::Rect draggedRect;
        static bool isDrag = false;
        static cv::Mat displayImg;
        
        // Lambda-like static wrapper or simple loop
        // cv::selectROI is blocking and only does Rect.
        // We need cv::setMouseCallback.
        
        std::string winName = "Select ROI (Drag) or Seed (Click)";
        cv::namedWindow(winName);
        displayImg = img.clone();
        
        cv::setMouseCallback(winName, [](int event, int x, int y, int flags, void* userdata) {
            if (event == cv::EVENT_LBUTTONDOWN) {
                clickedPt = cv::Point(x, y);
                isDrag = false;
            } else if (event == cv::EVENT_MOUSEMOVE && (flags & cv::EVENT_FLAG_LBUTTON)) {
                isDrag = true;
            } else if (event == cv::EVENT_LBUTTONUP) {
                if (!isDrag) {
                   std::cout << "Selected Point: " << clickedPt << "\n";
                   cv::circle(displayImg, clickedPt, 3, cv::Scalar(0,0,255), -1);
                   cv::imshow("Select ROI (Drag) or Seed (Click)", displayImg);
                }
            }
        }, NULL);
        
        cv::imshow(winName, img);
        int key = cv::waitKey(0);
        
        if (clickedPt.x != -1 && !isDrag) {
            interactiveSeed = clickedPt;
        } else {
             // Fallback to selectROI if they dragged (implementation complex here, so sticking to selectROI if usage implies ROI)
             // But wait, user specifically asked for point choice.
             // Let's rely on standard selectROI for Rect, but custom for Point?
             // Actually, selectROI returns a 1x1 rect if you just click? No.
        }
        cv::destroyWindow(winName);
        
        // If we got a point, use it.
        if (interactiveSeed.x != -1) {
             seedPt = interactiveSeed;
             // Default threshold if not set?
             if (seedStr.empty()) threshold = 20; 
        } else {
             // Retry with standard ROI
             roi = cv::selectROI("Select ROI (Fallback)", img);
        }
    } else if (!bboxStr.empty()) {
        roi = parseBBox(bboxStr);
    } else if (seedStr.empty()) {
        std::cerr << "Error: Must specify --bbox, --interactive, or --seed\n";
        return 1;
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

    cv::Mat binMask;

    // --- SEGMENTATION MODULE INTEGRATION ---
    avision::SegmentationModule segModule;
    binMask = segModule.segment(img, roi, seedPt);

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
