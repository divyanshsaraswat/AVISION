#include <jni.h>
#include <string>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <android/log.h>
#include "../../core/Engine.h"

// Android Logging Tag
#define TAG "AVisionNative"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// Global Engine Instance (Singleton-ish for the App lifecycle)
// Note: In a real production app, handle pointer validity strictly.
static std::unique_ptr<Engine> engine;
static std::shared_ptr<ICamera> dummyCamera; // Android pushes frames, so we don't need active capture
static std::shared_ptr<IAudio> dummyAudio;   // Android handles audio output

extern "C" JNIEXPORT jboolean JNICALL
Java_com_avision_app_NativeLib_init(JNIEnv* env, jobject /* this */, jstring modelPathStr) {
    LOGD("Initializing Native Engine...");

    // 1. Create Dummies (Platform Shells for Android will be implemented later if needed)
    // For Phase 3 MVP, we just need the Engine to process logic.
    // Audio alerts will be passed back to Java as return codes or callbacks in the future.
    
    // TODO: Implement AndroidCamera/AndroidAudio classes if we want C++ to control them directly.
    // For now, "Passive Mode" means we don't need them to drive the loop.
    
    // engine = std::make_unique<Engine>(nullptr, nullptr); // Requires specific dummy impls
    // For now, we need to refactor Engine ctor to accept nulls or create simple NullObjects
    
    // LOGD("Engine Initialized");
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_avision_app_NativeLib_processFrame(JNIEnv* env, jobject /* this */, 
                                            jlong matAddrInput, jlong matAddrDebug) {
    if (!engine) return JNI_FALSE;

    // Get OpenCV wrappers from Java pointers
    cv::Mat& frame = *(cv::Mat*)matAddrInput;
    cv::Mat& debug = *(cv::Mat*)matAddrDebug;

    if (frame.empty()) return JNI_FALSE;

    // Call Core
    // engine->processFrame(frame, debug);

    return JNI_TRUE;
}
