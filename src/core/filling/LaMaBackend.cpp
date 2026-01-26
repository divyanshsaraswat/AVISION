#include "LaMaBackend.h"
#include <iostream>
#include <filesystem>
#include <algorithm>

namespace avision {

LaMaBackend::LaMaBackend() {
    loadModel();
}

void LaMaBackend::loadModel() {
    std::vector<std::string> potentialPaths = {
        "models/big_lama.onnx",
        "build/Release/models/big_lama.onnx",
        "../models/big_lama.onnx",
        "../../models/big_lama.onnx"
    };

    for (const auto& path : potentialPaths) {
        if (std::filesystem::exists(path)) {
            modelPath = path;
            break;
        }
    }

    if (!modelPath.empty()) {
        try {
            std::cout << "[LaMaBackend] Loading model: " << modelPath << "\n";
            env = std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "BigLaMa");
            options = std::make_unique<Ort::SessionOptions>();
            options->SetIntraOpNumThreads(1);
            options->SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

            std::wstring wPath(modelPath.begin(), modelPath.end());
            session = std::make_unique<Ort::Session>(*env, wPath.c_str(), *options);
            allocator = std::make_unique<Ort::AllocatorWithDefaultOptions>();
            
            modelLoaded = true;
        } catch (const std::exception& e) {
            std::cerr << "[LaMaBackend] Failed to load model: " << e.what() << "\n";
            modelLoaded = false;
        }
    } else {
        std::cout << "[LaMaBackend] big_lama.onnx not found.\n";
    }
}

bool LaMaBackend::isAvailable() const {
    return modelLoaded;
}

cv::Mat LaMaBackend::inpaint(const cv::Mat& image, const cv::Mat& mask) {
    if (!modelLoaded) return cv::Mat();

    // 1. Prepare Inputs
    // LaMa expects [1, 3, H, W] image (normalized roughly to 0-1) and [1, 1, H, W] mask (0-1)
    // But check the export script normalization. Usually LaMa preprocessing is minimal or standard 0.5 mean.
    // Let's assume standard float32 [0,1].
    
    cv::Mat floatImg;
    image.convertTo(floatImg, CV_32FC3, 1.0/255.0);
    
    cv::Mat floatMask;
    mask.convertTo(floatMask, CV_32FC1, 1.0/255.0);
    // Threshold mask to be binary 0 or 1
    cv::threshold(floatMask, floatMask, 0.5, 1.0, cv::THRESH_BINARY);

    // HWC to CHW
    int h = floatImg.rows;
    int w = floatImg.cols;
    
    std::vector<float> inputImgValues;
    inputImgValues.reserve(1 * 3 * h * w);
    std::vector<cv::Mat> channels(3);
    cv::split(floatImg, channels);
    for(int i=0; i<3; ++i) 
        inputImgValues.insert(inputImgValues.end(), (float*)channels[i].data, (float*)channels[i].data + h*w);

    std::vector<float> inputMaskValues;
    inputMaskValues.assign((float*)floatMask.data, (float*)floatMask.data + h*w);

    std::vector<int64_t> shapeImg = {1, 3, h, w};
    std::vector<int64_t> shapeMask = {1, 1, h, w};

    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::vector<Ort::Value> inputTensors;
    inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, inputImgValues.data(), inputImgValues.size(), shapeImg.data(), shapeImg.size()));
    inputTensors.push_back(Ort::Value::CreateTensor<float>(memoryInfo, inputMaskValues.data(), inputMaskValues.size(), shapeMask.data(), shapeMask.size()));

    const char* inputNames[] = {"image", "mask"};
    const char* outputNames[] = {"output_image"};

    try {
        auto outputTensors = session->Run(Ort::RunOptions{nullptr}, inputNames, inputTensors.data(), 2, outputNames, 1);
        
        float* outData = outputTensors[0].GetTensorMutableData<float>();
        
        // Output is [1, 3, H, W]
        // CHW to HWC
        std::vector<cv::Mat> outChannels(3);
        int planeSize = h * w;
        outChannels[0] = cv::Mat(h, w, CV_32F, outData);
        outChannels[1] = cv::Mat(h, w, CV_32F, outData + planeSize);
        outChannels[2] = cv::Mat(h, w, CV_32F, outData + 2 * planeSize);
        
        cv::Mat merged;
        cv::merge(outChannels, merged);
        
        // Convert back to 8UC3
        merged.convertTo(merged, CV_8UC3, 255.0);
        return merged;

    } catch (const Ort::Exception& e) {
        std::cerr << "[LaMaBackend] Inference Failed: " << e.what() << "\n";
        return cv::Mat();
    }
}

} // namespace avision
