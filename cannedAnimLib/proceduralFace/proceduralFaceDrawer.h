#ifndef __Anki_Cozmo_ProceduralFaceDrawer_H__
#define __Anki_Cozmo_ProceduralFaceDrawer_H__

#include "coretech/common/shared/types.h"
#include "coretech/common/shared/math/matrix.h"
#include "cannedAnimLib/proceduralFace/proceduralFace.h"
#include "cannedAnimLib/proceduralFace/proceduralFaceModifierTypes.h"
#include "coretech/vision/engine/image.h"

namespace Anki {
  
  // Forward declaration:
  namespace Vision {
    class TrackedFace;
  }
 
  namespace Util {
    class RandomGenerator;
  } 
  
namespace Vector {

  class ProceduralFaceDrawer
  {
  public:

    //  The face rendering pipeline consists of stages, each depending on the previous stage,
    //  e.g. drawing each eye, transforming the face, adding scanlines, distortion, noise,
    //  with the final image converting to a RGB565 texture to be sent to the robot.
    //
    //  Each state in the pipeline follows a similar path:
    //  1) Did an earlier stage change the input to this stage?
    //       If yes, then this stage must render and clear the surface
    //       If no, then compare the latest face data with the face data used to generate
    //       the cached image, if the face data has changed this stage must render
    //  2) Render this stage
    //       Update the cached face data with the latest face data
    //       Use the face cache of the previous stage as input
    //       Assign a face cache for this stage as output
    //       Apply the stage
    //  3) Don't render this stage
    //       Leave the cached face data as it was
    //       Take the input from the previous stage and pass it on as the output from this
    //       stage, no image copy required
    //  4) Keep track of the latest face cache, this will be the final image
    //
    //  A key concept here is passing indexes into the cache for inputs and outputs, e.g.
    //  if there is no face transform then the input to the noise stage can be the output
    //  from the eyes stage. This is an alternative to the face transform performing a memcpy
    //  from its input to its output. All surfaces are I8 except the final, upon which
    //  last stage is converted from V (with constant H and S) to RGB565.
    //
    //  The code for each stage is kept largely as the original.
    //  Scanline distortion has changed from modifying the input image to generating a new
    //  output image, this allows eye rendering and face transforms to be cached and the
    //  scanline distortion applied to the original output rather than a face that has already
    //  had scanline distortion applied.
    //  Similarly for the noise.
    //
    //  ApplyScanlines is a special case as it is part of the public API and used elsewhere,
    //  its functionality has been retained and does not affect the face cache.

    // Closes eyes and switches interlacing. Call until it returns false, which
    // indicates there are no more blink frames and the face is back in its
    // original state. The output "offset" indicates the desired timing since
    // the previous state.
    static bool GetNextBlinkFrame(ProceduralFace& faceData, BlinkState& out_blinkState, TimeStamp_t& out_offset);
    
    // Actually draw the face with the current parameters
    static void DrawFace(const ProceduralFace& faceData, const Util::RandomGenerator& rng, Vision::ImageRGB565& output);
    
    // Applies scanlines to the input image.
    // Although the type of the input image is ImageRGB, it should be an HSV image, i.e.
    // the 'red' channel is hue, 'green' channel is saturation, and 'blue' channel is value

    static bool ApplyScanlines(Vision::ImageRGB& imageHsv, const float opacity, bool dirty = true);
    static bool ApplyScanlines(Vision::Image& image8, const float opacity, bool dirty = true);

  private:

    using Parameter = ProceduralEyeParameter;
    using WhichEye = ProceduralFace::WhichEye;
    using Value = ProceduralFace::Value;
    
    // Despite taking in an ImageRGB, note that this method actually draws in HSV and
    // is just using ImageRGB as a "3 channel image" since we don't (yet) have an ImageHSV.
    // The resulting face image is converted to RGB by DrawFace at the end.
    static void DrawEye(const ProceduralFace& faceData, WhichEye whichEye, const Matrix_3x3f* W_facePtr,
                        Vision::Image& faceHsv, Rectangle<f32>& eyeBoundingBox);
    
    static Matrix_3x3f GetTransformationMatrix(f32 angleDeg, f32 scaleX, f32 scaleY,
                                               f32 tX, f32 tY, f32 x0 = 0.f, f32 y0 = 0.f);
    
#if PROCEDURALFACE_GLOW_FEATURE
    static Vision::Image _glowImg;
#endif
    static Vision::Image _eyeShape;

    static struct FaceCache {
    public:
      // Stored face data, the data here was used to generate the cache values and images below
      ProceduralFace faceData;

      // Static images to do all our drawing in, the final image will be converted to RGB565
      // at the end. These is treated as an HSV images, potentially one per stage in the face
      // pipeline
      static const int kSize = 4;
      Vision::Image img8[kSize];
      Vision::ImageRGB565 img565;
      int eyes;
      int distortedFace;
      int finalFace;
    } _faceCache;

    // Bounding boxes, left eye, right eye, combined left/right eyes
    static Rectangle<f32> _leftBBox;
    static Rectangle<f32> _rightBBox;

    // Note 1: Not a Rectangle<s32> as SetX() and SetY() move the rectangle rather than resize it
    // Note 2: Any stage that modifies the bounding box is responsible for determining if there is NothingToDraw
    static s32 _faceColMin;
    static s32 _faceColMax;
    static s32 _faceRowMin;
    static s32 _faceRowMax;

    static void ApplyAntiAliasing(Vision::Image& shape, float minX, float minY, float maxX, float maxY);
    static bool DrawEyes(const ProceduralFace& faceData, bool dirty);
    static bool DistortScanlines(const ProceduralFace& faceData, bool dirty);
    static bool ApplyNoise(const Util::RandomGenerator& rng, bool dirty);
    static bool ConvertColorspace(const ProceduralFace& faceData, Vision::ImageRGB565& output, bool dirty);

#if PROCEDURALFACE_NOISE_FEATURE
    static const Array2d<u8>& GetNoiseImage(const Util::RandomGenerator& rng);
#endif

#if PROCEDURALFACE_SCANLINE_FEATURE
    // Returns true if scanline should be applied to the given row. Only
    // apply scanlines in alternating pairs of rows (i.e. 00110011)
    static bool ShouldApplyScanlineToRow(const u32 rowNum) { return (rowNum & 2) != 0; }
#endif
  }; // class ProceduralFaceπ
  
  
} // namespace Vector
} // namespace Anki

#endif // __Anki_Cozmo_ProceduralFaceDrawer_H__
