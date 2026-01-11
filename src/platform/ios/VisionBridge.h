#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>

NS_ASSUME_NONNULL_BEGIN

// iOS Bridge Class
@interface AVisionCore : NSObject

// Lifecycle
- (instancetype)init;
- (BOOL)initializeEngineWithModelPath:(NSString *)modelPath;
- (void)stopEngine;

// Frame Processing
// Takes a raw pixel buffer (from AVCaptureVideoDataOutput) and handles it.
// Returns a debug image (CMSampleBuffer or UIImage) if requested, or just processes logic.
- (void)processPixelBuffer:(CVPixelBufferRef)pixelBuffer;

@end

NS_ASSUME_NONNULL_END
