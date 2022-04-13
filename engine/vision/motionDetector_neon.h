/**
 * File: motionDetector_neon.h
 *
 * Author: Al Chaussee
 * Date:   1-25-2018
 *
 * Description: Implementations of neon optimized motion detector functions
 *
 * Copyright: Anki, Inc. 2017
 **/

#ifndef __Anki_Cozmo_Basestation_MotionDetector_Neon_H__
#define __Anki_Cozmo_Basestation_MotionDetector_Neon_H__

#include "engine/vision/motionDetector.h"

#include "coretech/common/shared/array2d.h"

namespace Anki {
namespace Vector {

#if defined(ANDROID) || defined(VICOS)
  
// Calculates the ratio of channel1 * (1 / channel2) and compares it to 
// kMotionThresh vector
// tempWhichAboveThresh1 and tempWhichAboveThresh2 will contain
// which elements of (channel1 / channel 2) are above the ratio threshold
//
// The following vectors are assumed to have already been created
// channel1 and channel2 are uint8x8_t vectors
// kMotionThresh is a float32x4
// tempWhichAboveThresh1/2 are uint32x4 vectors
#define PER_CHANNEL_RATIO(channel1, channel2) { \
    /* Expand channel2 from a uint8x8 to 2 uint32x4 vectors */ \
    uint16x8_t valueR16x8_2 = vmovl_u8(channel2); \
    uint16x4_t valueR16x4_2_1 = vget_low_u16(valueR16x8_2); \
    uint16x4_t valueR16x4_2_2 = vget_high_u16(valueR16x8_2); \
    uint32x4_t valueR32x4_2_1 = vmovl_u16(valueR16x4_2_1); \
    uint32x4_t valueR32x4_2_2 = vmovl_u16(valueR16x4_2_2); \
\
    /* Replace any 0s with 1s in the denominator to prevent dividing by 0 */ \
    valueR32x4_2_1 = vmaxq_u32(valueR32x4_2_1, kOnes); \
    valueR32x4_2_2 = vmaxq_u32(valueR32x4_2_2, kOnes); \
\
    /* Convert from u32 to f32 */ \
    float32x4_t valueRf32x4_2_1 = vcvtq_f32_u32(valueR32x4_2_1); \
    float32x4_t valueRf32x4_2_2 = vcvtq_f32_u32(valueR32x4_2_2); \
\
    /* Perform one iteration of the Newton-Raphson reciprocal method */ \
    valueRf32x4_2_1 = vrecpeq_f32(valueRf32x4_2_1); \
    valueRf32x4_2_2 = vrecpeq_f32(valueRf32x4_2_2); \
\
    /* Expand channel1 from a uint8x8 to 2 uint32x4 vectors */ \
    uint16x8_t valueR16x8_1 = vmovl_u8(channel1); \
    uint16x4_t valueR16x4_1_1 = vget_low_u16(valueR16x8_1); \
    uint16x4_t valueR16x4_1_2 = vget_high_u16(valueR16x8_1); \
    uint32x4_t valueR32x4_1_1 = vmovl_u16(valueR16x4_1_1); \
    uint32x4_t valueR32x4_1_2 = vmovl_u16(valueR16x4_1_2); \
\
    /* Convert u32 to f32 */ \
    float32x4_t valueRf32x4_1_1 = vcvtq_f32_u32(valueR32x4_1_1); \
    float32x4_t valueRf32x4_1_2 = vcvtq_f32_u32(valueR32x4_1_2); \
\
    /* Multiply channel1 * (1 / channel2) */ \
    valueRf32x4_1_1 = vmulq_f32(valueRf32x4_1_1, valueRf32x4_2_1); \
    valueRf32x4_1_2 = vmulq_f32(valueRf32x4_1_2, valueRf32x4_2_2); \
\
    /* Figure out which ratios are greater than kMotionThresh and OR them into the WhichAboveThresh vec */ \
    valueRf32x4_1_1 = vcgtq_f32(valueRf32x4_1_1, kMotionThresh); \
    uint32x4_t valueR_1_1 = vreinterpretq_u32_f32(valueRf32x4_1_1); \
    tempWhichAboveThresh1 = vorrq_u32(tempWhichAboveThresh1, valueR_1_1); \
\
    valueRf32x4_1_2 = vcgtq_f32(valueRf32x4_1_2, kMotionThresh); \
    uint32x4_t valueR_1_2 = vreinterpretq_u32_f32(valueRf32x4_1_2); \
    tempWhichAboveThresh2 = vorrq_u32(tempWhichAboveThresh2, valueR_1_2); } \

template<>
inline s32 MotionDetector::RatioTestNeonHelper<Vision::ImageRGB>(const u8*& imagePtr,
                                                                 const u8*& prevImagePtr,
                                                                 u8*& ratioImgPtr,
                                                                 u32 numElementsToProcess)
{
  s32 numAboveThresh = 0;

  const uint8x8_t   kMinBrightness = vdup_n_u8(kMotionDetection_MinBrightness);
  const uint8x8_t   kZeros         = vdup_n_u8(0);
  const uint32x4_t  kOnes          = vdupq_n_u32(1);
  const float32x4_t kMotionThresh  = vdupq_n_f32(kMotionDetection_RatioThreshold);

  const u32 kNumElementsProcessedPerLoop = 8;
  const u32 kSizeOfRGBElement = 3;
  const s32 kNumIterations = numElementsToProcess - (kNumElementsProcessedPerLoop - 1);

  s32 i;
  for(i = 0; i < kNumIterations; i += kNumElementsProcessedPerLoop)
  {
    // Load deinterleaved RGB data from the previous and current image
    uint8x8x3_t p1 = vld3_u8(imagePtr);
    imagePtr += kNumElementsProcessedPerLoop*kSizeOfRGBElement;
    uint8x8x3_t p2 = vld3_u8(prevImagePtr);
    prevImagePtr += kNumElementsProcessedPerLoop*kSizeOfRGBElement;

    // Compare all channels to figure out which elements are greater than min brightness 
    // in p1 (current image)
    uint8x8_t p1RgtMin = vcgt_u8(p1.val[0], kMinBrightness);
    uint8x8_t p1GgtMin = vcgt_u8(p1.val[1], kMinBrightness);
    uint8x8_t p1BgtMin = vcgt_u8(p1.val[2], kMinBrightness);

    // AND the results of the min brightness comparison for each element of each channel 
    // to figure out which pixels have all r, g, and b greater than min brightness
    //   p1.IsBrighterThan(kMotionDetection_MinBrightness)
    uint8x8_t p1gtMin = vand_u8(p1RgtMin, p1GgtMin);
    p1gtMin = vand_u8(p1gtMin, p1BgtMin);

    // Do the same comparison for p2 (previous image)
    uint8x8_t p2RgtMin = vcgt_u8(p2.val[0], kMinBrightness);
    uint8x8_t p2GgtMin = vcgt_u8(p2.val[1], kMinBrightness);
    uint8x8_t p2BgtMin = vcgt_u8(p2.val[2], kMinBrightness);

    uint8x8_t p2gtMin = vand_u8(p2RgtMin, p2GgtMin);
    p2gtMin = vand_u8(p2gtMin, p2BgtMin);

    // The corresponding pixels in both previous and current image need to be 
    // greater than min brightness
    //   p1.IsBrighterThan(kMotionDetection_MinBrightness) && 
    //   p2.IsBrighterThan(kMotionDetection_MinBrightness)
    uint8x8_t bothGtMin = vand_u8(p1gtMin, p2gtMin);

    // TODO? Loop if bothGtMin is all zeros

    // Zero out the elements that did not meet the above condition since
    // we will still be performing the ratio test on them and need the result to be 0
    p1.val[0] = vbsl_u8(bothGtMin, p1.val[0], kZeros);
    p1.val[1] = vbsl_u8(bothGtMin, p1.val[1], kZeros);
    p1.val[2] = vbsl_u8(bothGtMin, p1.val[2], kZeros);

    p2.val[0] = vbsl_u8(bothGtMin, p2.val[0], kZeros);
    p2.val[1] = vbsl_u8(bothGtMin, p2.val[1], kZeros);
    p2.val[2] = vbsl_u8(bothGtMin, p2.val[2], kZeros);

    // Setup value1 to be the numerator of the ratio and value2 to be the denominator
    // based on which channel of each pixel is larger in order to have a ratio that is
    // always >= 1
    uint8x8x3_t value1, value2;
    value1.val[0] = vmax_u8(p1.val[0], p2.val[0]);
    value2.val[0] = vmin_u8(p1.val[0], p2.val[0]);

    value1.val[1] = vmax_u8(p1.val[1], p2.val[1]);
    value2.val[1] = vmin_u8(p1.val[1], p2.val[1]);

    value1.val[2] = vmax_u8(p1.val[2], p2.val[2]);
    value2.val[2] = vmin_u8(p1.val[2], p2.val[2]);

    // These two vectors are the outputs of the PER_CHANNEL_RATIO macro
    // Element will be all 1s if the ratio in any channel is above RatioThreshold
    uint32x4_t tempWhichAboveThresh1 = vdupq_n_u32(0);
    uint32x4_t tempWhichAboveThresh2 = vdupq_n_u32(0);

    // Run the ratio macro on each pair of channels
    // Will output tempWhichAboveThresh1/2 which will contain which ratios
    // of any channel are above the motion threshold
    //   ratioR > kMotionDetection_RatioThreshold || 
    //   ratioG > kMotionDetection_RatioThreshold || 
    //   ratioB > kMotionDetection_RatioThreshold
    PER_CHANNEL_RATIO(value1.val[0], value2.val[0]);
    PER_CHANNEL_RATIO(value1.val[1], value2.val[1]);
    PER_CHANNEL_RATIO(value1.val[2], value2.val[2]);

    // Combine and narrow the two WhichAboveThresh vectors into one vector
    // Each element will either be all 1s (255) if the ratio of any channel of a given pair of pixels is
    // greater than the ratio threshold or all 0s
    uint16x4_t which1 = vmovn_u32(tempWhichAboveThresh1);
    uint16x4_t which2 = vmovn_u32(tempWhichAboveThresh2);
    uint16x8_t which = vcombine_u16(which1, which2);
    uint8x8_t pixelVal = vmovn_u16(which);

    // Write the ratio results to ratioImg
    vst1_u8(ratioImgPtr, pixelVal);
    ratioImgPtr += kNumElementsProcessedPerLoop;

    // Shift each element right 7 bits so 255 -> 1
    pixelVal = vshr_n_u8(pixelVal, 7);
    // Then repeatedly pairwise add elements to get the sum of all elements of
    // pixelVal which will be the number of pixels that are above the ratio threshold
    pixelVal = vpadd_u8(pixelVal, pixelVal);
    pixelVal = vpadd_u8(pixelVal, pixelVal);
    pixelVal = vpadd_u8(pixelVal, pixelVal);

    // All lanes of pixelVal will be the same so pull out one of them add it to numAboveThresh
    numAboveThresh += vget_lane_u8(pixelVal, 0);
  }

  const Vision::PixelRGB* imagePtr2     = reinterpret_cast<const Vision::PixelRGB*>(imagePtr);
  const Vision::PixelRGB* prevImagePtr2 = reinterpret_cast<const Vision::PixelRGB*>(prevImagePtr);
  
  // Process any extra elements one by one
  for(; i < numElementsToProcess; i++)
  {
    const Vision::PixelRGB& p1 = *imagePtr2;
    const Vision::PixelRGB& p2 = *prevImagePtr2;  

    if(p1.IsBrighterThan(kMotionDetection_MinBrightness) &&
       p2.IsBrighterThan(kMotionDetection_MinBrightness))
    {
      const f32 ratioR = RatioTestHelper(p1.r(), p2.r());
      const f32 ratioG = RatioTestHelper(p1.g(), p2.g());
      const f32 ratioB = RatioTestHelper(p1.b(), p2.b());
      if(ratioR > kMotionDetection_RatioThreshold || 
         ratioG > kMotionDetection_RatioThreshold || 
         ratioB > kMotionDetection_RatioThreshold) 
      {        
        ++numAboveThresh;
        *ratioImgPtr = 255; // use 255 because it will actually display
      }
      else
      {
        *ratioImgPtr = 0;
      }
    }

    imagePtr2++;
    prevImagePtr2++;
    ratioImgPtr++;
  }

  return numAboveThresh;
}

template<>
inline s32 MotionDetector::RatioTestNeonHelper<Vision::Image>(const u8*& imagePtr,
                                                              const u8*& prevImagePtr,
                                                              u8*& ratioImgPtr,
                                                              u32 numElementsToProcess)
{
  s32 numAboveThresh = 0;

  const uint8x8_t   kMinBrightness = vdup_n_u8(kMotionDetection_MinBrightness);
  const uint8x8_t   kZeros         = vdup_n_u8(0);
  const uint32x4_t  kOnes          = vdupq_n_u32(1);
  const float32x4_t kMotionThresh  = vdupq_n_f32(kMotionDetection_RatioThreshold);

  const u32 kNumElementsProcessedPerLoop = 8;
  const s32 kNumIterations = numElementsToProcess - (kNumElementsProcessedPerLoop - 1);

  s32 i;
  for(i = 0; i < kNumIterations; i += kNumElementsProcessedPerLoop)
  {
    // Load deinterleaved RGB data from the previous and current image
    uint8x8_t p1 = vld1_u8(imagePtr);
    imagePtr += kNumElementsProcessedPerLoop;
    uint8x8_t p2 = vld1_u8(prevImagePtr);
    prevImagePtr += kNumElementsProcessedPerLoop;

    // Compare all channels to figure out which elements are greater than min brightness 
    // in p1 (current image)
    uint8x8_t p1gtMin = vcgt_u8(p1, kMinBrightness);

    // Do the same comparison for p2 (previous image)
    uint8x8_t p2gtMin = vcgt_u8(p2, kMinBrightness);

    // The corresponding pixels in both previous and current image need to be 
    // greater than min brightness
    //   p1.IsBrighterThan(kMotionDetection_MinBrightness) && 
    //   p2.IsBrighterThan(kMotionDetection_MinBrightness)
    uint8x8_t bothGtMin = vand_u8(p1gtMin, p2gtMin);

    // TODO? Loop if bothGtMin is all zeros

    // Zero out the elements that did not meet the above condition since
    // we will still be performing the ratio test on them and need the result to be 0
    p1 = vbsl_u8(bothGtMin, p1, kZeros);
    p2 = vbsl_u8(bothGtMin, p2, kZeros);

    // Setup value1 to be the numerator of the ratio and value2 to be the denominator
    // based on which channel of each pixel is larger in order to have a ratio that is
    // always >= 1
    uint8x8_t value1 = vmax_u8(p1, p2);
    uint8x8_t value2 = vmin_u8(p1, p2);

    // These two vectors are the outputs of the PER_CHANNEL_RATIO macro
    // Element will be all 1s if the ratio in any channel is above RatioThreshold
    uint32x4_t tempWhichAboveThresh1 = vdupq_n_u32(0);
    uint32x4_t tempWhichAboveThresh2 = vdupq_n_u32(0);

    // Run the ratio macro on each pair of channels
    // Will output tempWhichAboveThresh1/2 which will contain which ratios
    // of any channel are above the motion threshold
    //   ratio > kMotionDetection_RatioThreshold
    PER_CHANNEL_RATIO(value1, value2);

    // Combine and narrow the two WhichAboveThresh vectors into one vector
    // Each element will either be all 1s (255) if the ratio of any channel of a given pair of pixels is
    // greater than the ratio threshold or all 0s
    uint16x4_t which1 = vmovn_u32(tempWhichAboveThresh1);
    uint16x4_t which2 = vmovn_u32(tempWhichAboveThresh2);
    uint16x8_t which = vcombine_u16(which1, which2);
    uint8x8_t pixelVal = vmovn_u16(which);

    // Write the ratio results to ratioImg
    vst1_u8(ratioImgPtr, pixelVal);
    ratioImgPtr += kNumElementsProcessedPerLoop;

    // Shift each element right 7 bits so 255 -> 1
    pixelVal = vshr_n_u8(pixelVal, 7);
    // Then repeatedly pairwise add elements to get the sum of all elements of
    // pixelVal which will be the number of pixels that are above the ratio threshold
    pixelVal = vpadd_u8(pixelVal, pixelVal);
    pixelVal = vpadd_u8(pixelVal, pixelVal);
    pixelVal = vpadd_u8(pixelVal, pixelVal);

    // All lanes of pixelVal will be the same so pull out one of them add it to numAboveThresh
    numAboveThresh += vget_lane_u8(pixelVal, 0);
  }
  
  // Process any extra elements one by one
  for(; i < numElementsToProcess; i++)
  {
    u8 p1 = *imagePtr;
    u8 p2 = *prevImagePtr;

    if(p1 > kMotionDetection_MinBrightness &&
       p2 > kMotionDetection_MinBrightness)
    {
      const f32 ratio = RatioTestHelper(p1, p2);
      if(ratio > kMotionDetection_RatioThreshold) 
      {        
        ++numAboveThresh;
        *ratioImgPtr = 255; // use 255 because it will actually display
      }
      else
      {
        *ratioImgPtr = 0;
      }
    }

    imagePtr++;
    prevImagePtr++;
    ratioImgPtr++;
  }

  return numAboveThresh;
}

template<class ImageType>
s32 MotionDetector::RatioTestNeon(const ImageType& image, Vision::Image& ratioImg)
{  
  s32 numAboveThresh = 0;

#if defined(ANDROID) || defined(VICOS)

  const bool isImageContinuous = image.IsContinuous();
  const bool isPrevImageContinuous = _prevImageRGB.IsContinuous();
  const bool isRatioImgContinuous = ratioImg.IsContinuous();

  u32 numRows = image.GetNumRows();
  u32 numElementsToProcessAtATime = image.GetNumCols();

  if(isImageContinuous && isPrevImageContinuous && isRatioImgContinuous)
  {
    numElementsToProcessAtATime *= numRows;
    numRows = 1;
  }

  for(int i = 0; i < numRows; i++)
  {
    const u8* imagePtr     = reinterpret_cast<const u8*>(image.GetRow(i));
    const u8* prevImagePtr = reinterpret_cast<const u8*>(_prevImageRGB.GetRow(i));
    u8*       ratioImgPtr  = reinterpret_cast<u8*>(ratioImg.GetRow(i));

    numAboveThresh += RatioTestNeonHelper<ImageType>(imagePtr, prevImagePtr, ratioImgPtr, numElementsToProcessAtATime);
  }

#endif

  return numAboveThresh;
}

#undef PER_CHANNEL_RATIO

#endif

}
}

#endif // __Anki_Cozmo_Basestation_MotionDetector_Neon_H__
