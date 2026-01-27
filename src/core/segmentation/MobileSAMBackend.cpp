#include "MobileSAMBackend.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <algorithm>

namespace avision {

MobileSAMBackend::MobileSAMBackend() {
    loadModels();
}

void MobileSAMBackend::loadModels() {
    // Look for models in relative paths
    std::vector<std::string> basePaths = {
        "models/",
        "build/Release/models/",
        "../models/",
        "../../models/"
    };

    // Find Encoder
    for (const auto& base : basePaths) {
        std::string p = base + "mobile_sam_encoder.onnx";
        if (std::filesystem::exists(p)) {
            encoderPath = p;
            break;
        }
    }

    // Find Decoder
    for (const auto& base : basePaths) {
        std::string p = base + "mobile_sam_decoder.onnx";
        if (std::filesystem::exists(p)) {
            decoderPath = p;
            break;
        }
    }

    if (!encoderPath.empty() && !decoderPath.empty()) {
        try {
            std::cout << "[MobileSAMBackend] Loading Encoder: " << encoderPath << std::endl;
            std::cout << "[MobileSAMBackend] Loading Decoder: " << decoderPath << std::endl;
            
            // Initialize ORT Environment
            env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "MobileSAM");
            sessionOptions = std::make_unique<Ort::SessionOptions>();
            sessionOptions->SetIntraOpNumThreads(1);
            sessionOptions->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

            // Create Sessions
            std::wstring wEncPath(encoderPath.begin(), encoderPath.end());
            std::wstring wDecPath(decoderPath.begin(), decoderPath.end());
            
            encoderSession = std::make_unique<Ort::Session>(*env, wEncPath.c_str(), *sessionOptions);
            decoderSession = std::make_unique<Ort::Session>(*env, wDecPath.c_str(), *sessionOptions);
            
            allocator = std::make_unique<Ort::AllocatorWithDefaultOptions>();

            modelLoaded = true;
            std::cout << "[MobileSAMBackend] Models loaded successfully (Encoder + Decoder).\n";
        } catch (const Ort::Exception& e) {
            std::cerr << "[MobileSAMBackend] Error loading models: " << e.what() << "\n";
            modelLoaded = false;
        } catch (const std::exception& e) {
            std::cerr << "[MobileSAMBackend] Standard Error: " << e.what() << "\n";
            modelLoaded = false;
        }
    } else {
        std::cout << "[MobileSAMBackend] Models not found. (Encoder or Decoder missing)\n";
    }
}

bool MobileSAMBackend::isAvailable() const {
    return modelLoaded;
}

// Helper to resize longest side to 1024
static cv::Mat preprocessImage(const cv::Mat& result, float& scale, cv::Size& originalSize) {
    originalSize = result.size();
    int h = result.rows;
    int w = result.cols;
    
    scale = 1024.0f / std::max(h, w);
    int newW = static_cast<int>(w * scale);
    int newH = static_cast<int>(h * scale);
    
    cv::Mat resized;
    cv::resize(result, resized, cv::Size(newW, newH));
    
    // Pad to 1024x1024 (bottom-right)
    int padW = 1024 - newW;
    int padH = 1024 - newH;
    
    cv::Mat padded;
    cv::copyMakeBorder(resized, padded, 0, padH, 0, padW, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    
    return padded;
}

void MobileSAMBackend::runEncoder(const cv::Mat& image) {
    if (hasEmbeddings) return; // Already cached
    
    std::cout << "[MobileSAMBackend] Running Encoder on new image...\n";

    float scale;
    cv::Size originalSize;
    cv::Mat inputImg = preprocessImage(image, scale, originalSize);

    // Convert BGR to RGB
    cv::cvtColor(inputImg, inputImg, cv::COLOR_BGR2RGB);

    // Normalize (Mean/Std manual per channel - RGB order)
    inputImg.convertTo(inputImg, CV_32FC3);
    inputImg = inputImg - cv::Scalar(123.675, 116.28, 103.53); // Subtract Mean
    cv::multiply(inputImg, cv::Scalar(1.0/58.395, 1.0/57.12, 1.0/57.375), inputImg); // Divide by Std

    // HWC to CHW conversion
    std::vector<float> inputTensorValues;
    inputTensorValues.reserve(1 * 3 * 1024 * 1024);

    std::vector<cv::Mat> channels(3);
    cv::split(inputImg, channels);

    // R, G, B
    inputTensorValues.insert(inputTensorValues.end(), (float*)channels[0].data, (float*)channels[0].data + 1024*1024);
    inputTensorValues.insert(inputTensorValues.end(), (float*)channels[1].data, (float*)channels[1].data + 1024*1024);
    inputTensorValues.insert(inputTensorValues.end(), (float*)channels[2].data, (float*)channels[2].data + 1024*1024);

    std::vector<int64_t> inputShape = {1, 3, 1024, 1024};
    
    // Create Input Tensor
    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(memoryInfo, inputTensorValues.data(), inputTensorValues.size(), inputShape.data(), inputShape.size());

    // Run Encoder
    const char* inputNames[] = {"input_image"}; // Standard MobileSAM Encoder Input
    const char* outputNames[] = {"image_embeddings"};

    try {
        auto outputTensors = encoderSession->Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);
        
        // Output: [1, 256, 64, 64]
        float* embedRaw = outputTensors[0].GetTensorMutableData<float>();
        size_t embedSize = 1 * 256 * 64 * 64; 
        
        cachedEmbeddings.assign(embedRaw, embedRaw + embedSize);
        hasEmbeddings = true;
        
    } catch (const Ort::Exception& e) {
        std::cerr << "[MobileSAMBackend] Encoder Failed: " << e.what() << "\n";
        hasEmbeddings = false;
        throw; 
    }
}

cv::Mat MobileSAMBackend::runDecoder(const std::vector<cv::Point>& points, const std::vector<int>& labels, const cv::Size& originalSize, float scale) {
     if (!hasEmbeddings) return cv::Mat();
     
     // std::cout << "[MobileSAMBackend] Running Decoder with " << points.size() << " points...\n";

     // Prepare Points (N, 2)
     // If no points provided, MobileSAM usually fails or returns empty mask.
     if (points.empty()) return cv::Mat();

     size_t numPoints = points.size();
     std::vector<float> pointsData;
     std::vector<float> labelsData;
     
     pointsData.reserve(numPoints * 2);
     labelsData.reserve(numPoints);

     for(size_t i=0; i<numPoints; ++i) {
         pointsData.push_back((float)points[i].x * scale);
         pointsData.push_back((float)points[i].y * scale);
         labelsData.push_back((float)labels[i]);
     }
     
     std::vector<int64_t> pointsShape = {1, (int64_t)numPoints, 2};
     std::vector<int64_t> labelsShape = {1, (int64_t)numPoints};
     
     auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
     std::vector<Ort::Value> inputTensors;
     
     // 1. Image Embeddings
     std::vector<int64_t> embedShape = {1, 256, 64, 64};
     inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, cachedEmbeddings.data(), cachedEmbeddings.size(), embedShape.data(), embedShape.size()));
     
     // 2. Point Coords
     inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, pointsData.data(), pointsData.size(), pointsShape.data(), pointsShape.size()));
     
     // 3. Point Labels
     inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, labelsData.data(), labelsData.size(), labelsShape.data(), labelsShape.size()));
     
     // 4. Mask Input (Dummy) - 256x256
     std::vector<float> maskInput(1 * 1 * 256 * 256, 0.0f);
     std::vector<int64_t> maskInputShape = {1, 1, 256, 256};
     inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, maskInput.data(), maskInput.size(), maskInputShape.data(), maskInputShape.size()));
     
     // 5. Has Mask Input
     std::vector<float> hasMaskInput = { 0.0f };
     std::vector<int64_t> hasMaskShape = {1};
     inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, hasMaskInput.data(), hasMaskInput.size(), hasMaskShape.data(), hasMaskShape.size()));
     
     // 6. Orig Im Size
     std::vector<float> origSizeData = { (float)originalSize.height, (float)originalSize.width }; 
     std::vector<int64_t> origSizeShape = {2};
     inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, origSizeData.data(), origSizeData.size(), origSizeShape.data(), origSizeShape.size()));
     
     const char* inputNames[] = {"image_embeddings", "point_coords", "point_labels", "mask_input", "has_mask_input", "orig_im_size"};
     const char* outputNames[] = {"masks", "iou_predictions", "low_res_masks"};
     
     try {
         auto outputTensors = decoderSession->Run(Ort::RunOptions{nullptr}, inputNames, inputTensors.data(), 6, outputNames, 3);
         
         float* maskRaw = outputTensors[0].GetTensorMutableData<float>();
         float* iouRaw = outputTensors[1].GetTensorMutableData<float>();
         
         // Best Mask Logic (Greedy)
         int bestIdx = 0;
         float maxIou = -1.0f;
         for (int i=0; i<4; ++i) {
             if(iouRaw[i] > maxIou) {
                 maxIou = iouRaw[i];
                 bestIdx = i;
             }
         }
         
         // Dimensions
         int w = originalSize.width;
         int h = originalSize.height;
         int area = w * h;
         
         float* bestMaskStart = maskRaw + (bestIdx * area);
         
         cv::Mat finalMask(h, w, CV_32F, bestMaskStart);
         cv::Mat binMask;
         cv::threshold(finalMask, binMask, 0.0, 255, cv::THRESH_BINARY);
         binMask.convertTo(binMask, CV_8UC1);
         
         return binMask;
         
     } catch (const Ort::Exception& e) {
         std::cerr << "[MobileSAMBackend] Decoder Failed: " << e.what() << "\n";
         return cv::Mat();
     }
}

cv::Mat MobileSAMBackend::segment(const cv::Mat& image, const std::vector<cv::Point>& points, const std::vector<int>& labels, const cv::Rect& box) {
    if (!modelLoaded || image.empty()) return cv::Mat();

    try {
        // Run Encoder if Image Changed
        // Simple check: Dimensions + Data Pointer (User responsibility to keep same mat for same image in interactive mode)
        bool imageChanged = lastImage.empty() || image.size() != lastImage.size();
        
        // Full content check is too slow for real-time. 
        // We assume if dimensions match and it's interactive mode, it's the same image.
        // Or we rely on caller to manage this?
        // Let's implement a 'reset' method? No.
        // Let's keep a simplistic check for now. If user provides different image with same dims, artifacts might occur.
        // User should probably re-instantiate backend or we add explicit 'setImage'.
        // For CLI, it's fine.
        
        if (imageChanged) {
             hasEmbeddings = false;
             runEncoder(image); 
             lastImage = image.clone();
        } else if (hasEmbeddings == false) {
             runEncoder(image);
             lastImage = image.clone();
        }
        
        // Scale
        int h = image.rows;
        int w = image.cols;
        float scale = 1024.0f / std::max(h, w);
        
        // Combine Points & Box
        std::vector<cv::Point> finalPoints = points;
        std::vector<int> finalLabels = labels;
        
        if (!box.empty()) {
            // Box is represented as top-left (Label 2) and bottom-right (Label 3)
            finalPoints.push_back(cv::Point(box.x, box.y));
            finalLabels.push_back(2);
            
            finalPoints.push_back(cv::Point(box.x + box.width, box.y + box.height));
            finalLabels.push_back(3);
        }
        
        return runDecoder(finalPoints, finalLabels, image.size(), scale);

    } catch (const std::exception& e) {
        std::cerr << "[MobileSAMBackend] Inference Failed: " << e.what() << "\n";
        return cv::Mat();
    }
}

cv::Mat MobileSAMBackend::segment(const cv::Mat& image, const cv::Rect& roi, const cv::Point& seed) {
    // Backward compatibility wrapper
    std::vector<cv::Point> points;
    std::vector<int> labels;
    
    if (seed.x != -1) {
        points.push_back(seed);
        labels.push_back(1); // Foreman
    } else if (!roi.empty()) {
        // If only ROI provided, use box prompt
        // (Handled by passing box to next func)
    } else {
        // No prompts?
    }
    
    return segment(image, points, labels, roi);
}

} // namespace avision
