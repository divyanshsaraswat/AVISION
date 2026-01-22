#include "ImageCLI.h"
#include <iostream>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <sstream>

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

    if (inputPath.empty() || outputPath.empty()) {
        std::cerr << "Usage: avision image segment --input <img.jpg> (--bbox x,y,w,h | --seed x,y[,thresh] | --interactive) --output <mask.png>\n";
        return 1;
    }

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

    if (roi.area() == 0 && seedPt.x == -1 && !interactive) {
        std::cerr << "Error: Must specify --bbox, --interactive, or --seed\n";
        return 1;
    }

    cv::Mat binMask;

    // SCENARIO 1: Smart Click (Point-based GrabCut)
    if (seedPt.x != -1) {
        if (seedPt.x < 0 || seedPt.x >= img.cols || seedPt.y < 0 || seedPt.y >= img.rows) {
            std::cerr << "Error: Seed point out of bounds\n";
            return 1;
        }

        std::cout << "[Segment] Running Point-Initialized GrabCut (Classical)... please wait.\n";

        // Initialize Mask with PROBABLE FOREGROUND
        // We assume the user clicked an object that likely occupies a good chunk of the image.
        // If we start with PR_BGD, GrabCut is too lazy to expand.
        cv::Mat mask(img.size(), CV_8UC1, cv::Scalar(cv::GC_PR_FGD));

        // 1. Mark the seed point as SURE Foreground (Anchor)
        cv::circle(mask, seedPt, 5, cv::Scalar(cv::GC_FGD), -1);

        // 2. Mark borders as SURE Background (Constraint)
        // Critical: We must define what is definitely NOT the object.
        // We assume the object is not touching ALL borders.
        int border = 2; // 2px border
        cv::rectangle(mask, cv::Point(0,0), cv::Point(img.cols-1, img.rows-1), cv::Scalar(cv::GC_BGD), border);
        
        // 3. Mark corners aggressively? No, simple border is enough for now.

        // 3. Run GrabCut
        cv::Mat bgModel, fgModel;
        try {
            // Increase iterations to 7 for better convergence
            cv::grabCut(img, mask, cv::Rect(), bgModel, fgModel, 7, cv::GC_INIT_WITH_MASK);
        } catch (const cv::Exception& e) {
             std::cerr << "GrabCut Error: " << e.what() << "\n";
             return 1;
        }

        // Convert result: FG + PR_FGD -> 255
        binMask = (mask == cv::GC_PR_FGD) | (mask == cv::GC_FGD);
        binMask = binMask * 255; 
        
        std::cout << "[Segment] Segmentation Complete.\n";
    }
    // SCENARIO 2: GrabCut (ROI)
    else {
        if (interactive) {
            roi = cv::selectROI("Select ROI (Space/Enter to confirm)", img);
            cv::destroyWindow("Select ROI (Space/Enter to confirm)");
        } else {
           // roi already parsed
        }

        if (roi.area() == 0) {
             std::cerr << "Error: Invalid ROI\n";
             return 1;
        }

        // Deterministic Segmentation (GrabCut)
        cv::Mat mask = cv::Mat::zeros(img.size(), CV_8UC1);
        cv::Mat bgModel, fgModel;
        
        cv::grabCut(img, mask, roi, bgModel, fgModel, 5, cv::GC_INIT_WITH_RECT);

        // Convert GrabCut classes to binary mask
        cv::Mat finalMask = (mask == cv::GC_PR_FGD) | (mask == cv::GC_FGD);
        binMask = finalMask * 255; 
    }

    // Resolve Output Path (Default to next to input or CWD?) 
    // User requested "root file to access... in build/Release".
    // If output path is just filename, maybe put it next to exe too?
    // Let's keep output strict unless relative.
    
    // Actually, let's allow CWD output but if input was resolved to exe dir, maybe user expects output there?
    // Let's stick to CWD for output unless fully specified, BUT verify if this matches "root file access" request.
    // The request likely meant INPUT files.

    // Save
    if (cv::imwrite(outputPath, binMask)) {
        std::cout << "[Segment] Saved mask to: " << outputPath << "\n";
        return 0;
    } else {
        std::cerr << "Error: Failed to write output\n";
        return 1;
    }
}

int ImageCLI::handleFill(int argc, char** argv) {
    std::string inputPath, maskPath, outputPath;
    std::string mode = "remove"; // default
    bool overwrite = false;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input" && i+1 < argc) inputPath = argv[++i];
        else if (arg == "--mask" && i+1 < argc) maskPath = argv[++i];
        else if (arg == "--output" && i+1 < argc) outputPath = argv[++i];
        else if (arg == "--mode" && i+1 < argc) mode = argv[++i];
        else if (arg == "--overwrite") overwrite = true;
    }

    if (inputPath.empty() || maskPath.empty() || outputPath.empty()) {
         std::cerr << "Usage: avision image fill --input <img.jpg> --mask <mask.png> --output <filled.jpg>\n";
         std::cerr << "Generative fill is intentionally deferred to a future release.\n";
         return 1;
    }
    
    if (!overwrite && fileExists(outputPath)) {
        std::cerr << "Error: Output file exists (use --overwrite)\n";
        return 1;
    }

    cv::Mat img = cv::imread(inputPath);
    cv::Mat mask = cv::imread(maskPath, cv::IMREAD_GRAYSCALE); // Read as simple mask

    if (img.empty() || mask.empty()) {
        std::cerr << "Error: Could not open input or mask.\n";
        return 1;
    }
    
    if (img.size() != mask.size()) {
        std::cerr << "Error: Image and Mask dimensions do not match.\n";
        return 1;
    }

    cv::Mat result;
    double radius = 3.0;
    int flags = cv::INPAINT_TELEA;

    if (mode == "repair") {
        flags = cv::INPAINT_NS; // Navier-Stokes often better for structural repair
        radius = 5.0;
    }

    cv::inpaint(img, mask, result, radius, flags);

    if (cv::imwrite(outputPath, result)) {
        std::cout << "[Fill] Saved filled image to: " << outputPath << "\n";
        return 0;
    } else {
        std::cerr << "Error: Failed to write output\n";
        return 1;
    }
}
