/**
 * File: GroundPlaneClassifier.h
 *
 * Author: Lorenzo Riano
 * Created: 11/29/17
 *
 * Description: The GroundPlaneClassifier uses the RawPixelsClassifier to classify the ground plane as either drivable
 * or not. It interfaces directly with the VisionSystem. Classes to extract features are also provided.
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __Anki_Cozmo_Basestation_GroundplaneClassifier_H__
#define __Anki_Cozmo_Basestation_GroundplaneClassifier_H__

#include "coretech/common/engine/math/polygon_fwd.h"
#include "coretech/common/shared/types.h"
#include "coretech/vision/engine/debugImageList.h"
#include "coretech/vision/engine/image.h"
#include "coretech/vision/engine/profiler.h"
#include "engine/vision/rawPixelsClassifier.h"
#include "engine/vision/visionPoseData.h"

namespace Anki {
namespace Vector {

// Forward declaration
class CozmoContext;
struct OverheadEdgeFrame;

/****************************************************************
 *                     Features Extractors                      *
 ****************************************************************/

/**
 * Generic interface class to extract features from an image. It uses the type FeatureType defined in RawPixelsClassifier
 */
class IFeaturesExtractor {
public:

  /* Base class should declare a virtual destructor so derived class destructors will be called by delete */
  virtual ~IFeaturesExtractor() = default;

  /**
   * Extract features from a single pixel in the image in position (row, col)
   * @return the features
   */
  virtual std::vector<RawPixelsClassifier::FeatureType>
  Extract(const Vision::ImageRGB& image, int row, int col) const = 0;

  /**
   * Calculates the features over the whole image and returns them in an array.
   */
  virtual Array2d<RawPixelsClassifier::FeatureType>
  Extract(const Vision::ImageRGB& image) const = 0;

};

/**
 * For each pixel, the features are the mean (per channel) of the neighbour pixels.
 */
class MeanFeaturesExtractor : public IFeaturesExtractor {
public:

  /**
   * @param padding specifies half the size of the square around the pixel to use to compute the mean.
   * The square will have an edge of 2*padding+1 length. For example with padding=1 the square will be 3x3.
   */
  explicit MeanFeaturesExtractor(int padding) : _padding(padding), _profiler("MeanFeaturesExtractor"){}

  Array2d<RawPixelsClassifier::FeatureType> Extract(const Vision::ImageRGB& image) const override;

  std::vector<RawPixelsClassifier::FeatureType>
  Extract(const Vision::ImageRGB& image, int row, int col) const override;

private:
  int _padding;
  // These parameters are mutable since MeanFeaturesExtractor calculates the mean image once at the beginning, caches it
  // and accesses it for subsequent  calls. Better to use Extract that takes the whole image.
  mutable uchar* _prevImageData = nullptr; // used to check if a new image is being used
  mutable cv::Mat _meanImage;
  mutable cv::MatIterator_<cv::Vec3f> _meanMatIterator;
  mutable Anki::Vision::Profiler _profiler;
};

/**
 * Returns a pixel as its own feature
 */
class SinglePixelFeaturesExtraction : public IFeaturesExtractor {
public:
  std::vector<RawPixelsClassifier::FeatureType> Extract(const Vision::ImageRGB& image, int row, int col) const override;

  Array2d<RawPixelsClassifier::FeatureType> Extract(const Vision::ImageRGB& image) const override;
};

/****************************************************************
 *                     Helper Functions                         *
 ****************************************************************/

void ClassifyImage(const RawPixelsClassifier& clf, const Anki::Vector::IFeaturesExtractor& extractor,
                   const Vision::ImageRGB& image, Vision::Image& outputMask);


template<typename T1, typename T2>
void CVMatToVector(const cv::Mat& mat, std::vector<std::vector<T2>>& vec)
{
  DEV_ASSERT(mat.type() == cv::DataType<T1>::type, "CVMatToVector.WrongMatrixType");
  DEV_ASSERT(mat.channels() == 1, "CVMatToVector.WrongNumberOfChannels");

  int nRows = mat.rows;
  int nCols = mat.cols;
  vec = std::vector<std::vector<T2>>(nRows, std::vector<T2>(nCols));

  for(int i = 0; i < nRows; ++i)
  {
    const T1* p = mat.ptr<T1>(i);
    std::vector<T2>& singleRow  = vec[i];
    for (int j = 0; j < nCols; ++j)
    {
      singleRow[j] = cv::saturate_cast<T2>(p[j]);
    }
  }
}

template<typename T1, typename T2>
void CVMatToVector(const cv::Mat& mat, std::vector<T2>& vec)
{
  DEV_ASSERT(mat.type() == cv::DataType<T1>::type, "CVMatToVector.WrongMatrixType");
  DEV_ASSERT(mat.rows == 1, "CVMatToVector.OnlySingleRowAllowed");
  DEV_ASSERT(mat.channels() == 1, "CVMatToVector.WrongNumberOfChannels");

  int nCols = mat.cols;
  vec = std::vector<T2>(nCols);

  const T1* p = mat.ptr<T1>(0);
  for (int j = 0; j < nCols; ++j)
  {
    vec[j] = cv::saturate_cast<T2>(p[j]);
  }

}

/****************************************************************
 *                    Ground Plane Classifier                   *
 ****************************************************************/

/**
 * Class to interface the VisionSystem with the RawPixelsClassifier. At the moment it uses a DTRawPixelsClassifier but
 * it can accept any RawPixelsClassifier. The classifier can either be serialized and later loaded or trained on the fly.
 */
class GroundPlaneClassifier
{
public:
  GroundPlaneClassifier(const Json::Value& config, const CozmoContext *context);

  Result Update(const Vision::ImageRGB& image, const VisionPoseData& poseData,
                Vision::DebugImageList<Vision::CompressedImage>& debugImages,
                std::list<OverheadEdgeFrame>& outEdges);

  bool IsInitialized() const {
    return _initialized;
  }

  const RawPixelsClassifier& GetClassifier() const {
    DEV_ASSERT(_classifier.get() != nullptr, "GroundPlaneClassifier.GetClassifier.NULLClassifier");
    return *(_classifier.get());
  }

  Vision::Image ProcessClassifiedImage(const Vision::Image& binaryImage) const;

protected:
  std::unique_ptr<RawPixelsClassifier> _classifier;
  std::unique_ptr<IFeaturesExtractor> _extractor;
  const CozmoContext* _context;
  bool _initialized = false;
  Anki::Vision::Profiler _profiler;

  void TrainClassifier(const std::string& path);
  bool LoadClassifier(const std::string& filename);

};

} // namespace Anki
} // namespace Vector


#endif //__Anki_Cozmo_Basestation_GroundplaneClassifier_h
