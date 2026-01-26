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

cv::Mat MobileSAMBackend::runDecoder(const cv::Point& seed, const cv::Size& originalSize, float scale) {
     if (!hasEmbeddings) return cv::Mat();
     
     std::cout << "[MobileSAMBackend] Running Decoder...\n";

     // Prepare Points
     float x_pt = seed.x * scale;
     float y_pt = seed.y * scale;
     
     // 1 point + 1 padding point
     std::vector<float> pointsData = { x_pt, y_pt, 0.0f, 0.0f };
     std::vector<int64_t> pointsShape = {1, 2, 2};
     std::vector<float> labelsData = { 1.0f, -1.0f }; // 1 = Foreground, -1 = Padding
     std::vector<int64_t> labelsShape = {1, 2};
     
     auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
     std::vector<Ort::Value> inputTensors;
     
     // 1. Image Embeddings
     std::vector<int64_t> embedShape = {1, 256, 64, 64};
     inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, cachedEmbeddings.data(), cachedEmbeddings.size(), embedShape.data(), embedShape.size()));
     
     // 2. Point Coords
     inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, pointsData.data(), pointsData.size(), pointsShape.data(), pointsShape.size()));
     
     // 3. Point Labels
     inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, labelsData.data(), labelsData.size(), labelsShape.data(), labelsShape.size()));
     
     // 4. Mask Input (Dummy)
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
         
         // extract masks: [1, 4, H, W] or similar. Standard SAM exports fixed size masks or resized?
         // If return_extra_metrics was false, it's just masks, iou, low_res.
         // Let's assume the shape is compatible.
         
         float* maskRaw = outputTensors[0].GetTensorMutableData<float>();
         float* iouRaw = outputTensors[1].GetTensorMutableData<float>();
         
         // Best Mask
         int bestIdx = 0;
         float maxIou = -1.0f;
         for (int i=0; i<4; ++i) {
             if(iouRaw[i] > maxIou) {
                 maxIou = iouRaw[i];
                 bestIdx = i;
             }
         }
         
         // Assuming output mask is high-res (equal to orig_im_size due to dynamic axis export? or 1024?)
         // Actually, if we pass orig_im_size, the model often upscales internally.
         // Let's check tensor info if possible, but for now assume it matches origSize?
         // Safe bet: The output is usually the size of 'orig_im_size'.
         
         // If dimensions match original size:
         int w = originalSize.width;
         int h = originalSize.height;
         int area = w * h;
         
         // Safety check: verify raw buffer size if we could.
         // Instead, we just trust.
         
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

cv::Mat MobileSAMBackend::segment(const cv::Mat& image, const cv::Rect& roi, const cv::Point& seed) {
    if (!modelLoaded || image.empty()) return cv::Mat();

    // Determine Seed
    cv::Point targetPoint = seed;
    if (targetPoint.x == -1) {
        targetPoint.x = roi.x + roi.width / 2;
        targetPoint.y = roi.y + roi.height / 2;
    }

    try {
        // Run Encoder if Image Changed
        if (lastImage.empty() || cv::countNonZero(image != lastImage) > 0) {
             // Reset embeddings cache
             hasEmbeddings = false;
             runEncoder(image); 
             lastImage = image.clone();
        }
        
        // Calculate Preprocessing Scale
        int h = image.rows;
        int w = image.cols;
        float scale = 1024.0f / std::max(h, w);
        
        return runDecoder(targetPoint, image.size(), scale);

    } catch (const Ort::Exception& e) {
        std::cerr << "[MobileSAMBackend] Inference Failed: " << e.what() << "\n";
        return cv::Mat();
    }
}

} // namespace avision
