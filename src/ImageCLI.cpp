#include "ImageCLI.h"
#include <iostream>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <filesystem>

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
    std::string inputPath, outputPath, bboxStr;
    bool interactive = false;
    bool overwrite = false;

    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input" && i+1 < argc) inputPath = argv[++i];
        else if (arg == "--output" && i+1 < argc) outputPath = argv[++i];
        else if (arg == "--bbox" && i+1 < argc) bboxStr = argv[++i];
        else if (arg == "--interactive") interactive = true;
        else if (arg == "--overwrite") overwrite = true;
    }

    if (inputPath.empty() || outputPath.empty()) {
        std::cerr << "Usage: avision image segment --input <img.jpg> (--bbox x,y,w,h | --interactive) --output <mask.png>\n";
        return 1;
    }

    if (!overwrite && fileExists(outputPath)) {
        std::cerr << "Error: Output file exists (use --overwrite)\n";
        return 1;
    }

    cv::Mat img = cv::imread(inputPath);
    if (img.empty()) {
        std::cerr << "Error: Could not read valid input image\n";
        return 1;
    }

    cv::Rect roi;
    if (interactive) {
        roi = cv::selectROI("Select ROI (Space/Enter to confirm)", img);
        cv::destroyWindow("Select ROI (Space/Enter to confirm)");
    } else if (!bboxStr.empty()) {
        roi = parseBBox(bboxStr);
    } else {
        std::cerr << "Error: Must specify --bbox or --interactive\n";
        return 1;
    }

    if (roi.area() == 0) {
        std::cerr << "Error: Invalid ROI\n";
        return 1;
    }

    // Deterministic Segmentation (GrabCut)
    cv::Mat mask = cv::Mat::zeros(img.size(), CV_8UC1);
    cv::Mat bgModel, fgModel;
    
    // GC_INIT_WITH_RECT implies we trust the rect to contain the object
    cv::grabCut(img, mask, roi, bgModel, fgModel, 5, cv::GC_INIT_WITH_RECT);

    // Convert GrabCut classes (0=BG, 1=FG, 2=PR_BG, 3=PR_FG) to binary mask
    cv::Mat binMask;
    // Set FG and ProbableFG to 255, others to 0
    binMask = (mask == cv::GC_PR_FGD) | (mask == cv::GC_FGD);
    // Convert logic 0/1 result to 0/255
    binMask = binMask * 255; 

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
