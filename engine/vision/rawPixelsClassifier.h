/**
 * File: rawPixelsClassifier.h
 *
 * Author: Lorenzo Riano
 * Created: 11/9/17
 *
 * Description: A set of classes to classify pixels. Used mainly by GroundClassifier
 *              See testSurfaceClassifier.cpp for examples of use.
 *
 * Copyright: Anki, Inc. 2017
 *
 **/

#ifndef __Anki_Cozmo_RawPixelsClassifier_H__
#define __Anki_Cozmo_RawPixelsClassifier_H__


#include "coretech/common/engine/jsonTools.h"
#include "engine/vision/overheadMap.h"

#include <opencv2/core.hpp>

// forward declarations
namespace cv {
namespace ml {
class EM;
class LogisticRegression;
class TrainData;
class DTrees;
}
}

namespace Anki {

class WeightedLogisticRegression;

namespace Vision{
class Profiler;
}

namespace Vector {

class CozmoContext;

/****************************************************************
 *                     RawPixelsClassifier                 *
 ****************************************************************/
/*
 * Generic class for drivable surface classifier. See Individual implementations below. The class can be
 * trained either from data (see Train) or from files (see TrainFromFiles). To classify a (set of) pixel
 * use the Predict methods. The pixels are provided by the OverheadMap class. Training is performed in
 * batches.
 */
class RawPixelsClassifier {

public:

  typedef float FeatureType;

  explicit RawPixelsClassifier(const CozmoContext *context, Anki::Vision::Profiler* profiler = nullptr);

  /* Base class should declare a virtual destructor so derived class destructors will be called by delete */
  virtual ~RawPixelsClassifier() = default;

  /*
   * Train from a set of drivable and non-drivable pixels. Data might come from OverheadMap
   */
  bool Train(const OverheadMap::PixelSet& drivablePixels, const OverheadMap::PixelSet& nonDrivablePixels);

  /*
   * This function is used mainly for testing the class and debugging
   */
  void GetTrainingData(cv::Mat& trainingSamples, cv::Mat& trainingLabels) const;
  void SetTrainingData(const cv::Mat& trainingSamples, const cv::Mat& trainingLabels);

  /*
   * Predict the class of a single pixel (1 is drivable, 0 is not). Implementation must be provided by subclass.
   */
  virtual uchar PredictClass(const std::vector<FeatureType>& values) const = 0;

  /*
   * Predict the class of a vector of pixels (1 is drivable, 0 is not)
   */
  virtual std::vector<uchar> PredictClass(const std::vector<std::vector<FeatureType>>& features) const;

  /*
   * This overloaded function is less efficient than the one using vector<vector>.
   * It's meant to be overriden by a subclasses, which might have a more efficient implementation
   */
  virtual std::vector<uchar> PredictClass(const Anki::Array2d<FeatureType>& features) const;

  /*
   * Load data from two files and use it for training
   */
  virtual bool TrainFromFiles(const char* positiveDataFileName, const char* negativeDataFileName);

  /*
   * Serialize/Deserialize the detector
   */
  virtual bool Serialize(const char* filename) = 0;
  virtual bool DeSerialize(const char* filename) = 0;

protected:
  cv::Mat _trainingSamples;
  cv::Mat _trainingLabels;
  const CozmoContext* _context;
  Anki::Vision::Profiler* _profiler;

  virtual bool Train(const cv::Mat& allInputs, const cv::Mat& allClasses, uint numberOfPositives) = 0;

  /*
   * Write a cv::Mat to a file
   */
  void WriteMat(const cv::Mat& mat, const char *filename) const;

  template<class T>
  void WriteMat(const cv::Mat& mat, std::ofstream& outputFile) const;
};

/****************************************************************
 *                     GMMRawPixelsClassifier              *
 ****************************************************************/

/*
 * Abstract class for drivable surface classification using Gaussian Mixture Models. See LRRawPixelsClassifier
 * and THRawPixelsClassifier for implementations
 */
class GMMRawPixelsClassifier : public RawPixelsClassifier{
public:
  explicit GMMRawPixelsClassifier(const Json::Value& config, const CozmoContext *context);

protected:
  cv::Ptr<cv::ml::EM> _gmm;

  /*
   * For each row in input, calculate the mimimum distance from the kernels in the Gaussian Mixture model and return it
   * If useWeight=true then scale the distance by the weight of the kernel, so lower weight kernels will have a
   * greater distance.
   */
  std::vector<float> MinMahalanobisDistanceFromGMM(const cv::Mat& input, bool useWeight = true) const;

  bool TrainGMM(const cv::Mat& input);

};

/****************************************************************
 *                     LRRawPixelsClassifier               *
 ****************************************************************/
/*
 * This class uses Gaussian Mixture Models (GMM) and Logistic Regression (LR) to classify pixels as drivable or not.
 * To classify/train a set of pixels, a 3-dimensional GMM is created with only the positive (drivable surface) pixels.
 * Then for each pixel its minimum Mahalanobis distance from all the kernels is computed. This new one dimensional
 * vector is used as training for LR which finally classifies the pixel as drivable or not.
 *
 * Since the OverheadMap only labels pixels the robot has driven on as positive, a higher weight should be given to the
 * positive (1) class during training.
 */
class LRRawPixelsClassifier : public GMMRawPixelsClassifier
{
public:
  explicit LRRawPixelsClassifier(const Json::Value& config, const CozmoContext *context);

  using GMMRawPixelsClassifier::PredictClass;
  uchar PredictClass(const std::vector<FeatureType>& values) const override;

  bool Serialize(const char *filename) override
  {
    PRINT_NAMED_ERROR("LRRawPixelsClassifier.SerializeNotImplemented", "Serialize is not implemented for "
                                                                            "LRRawPixelsClassifier");
    return false;
  }

  bool DeSerialize(const char *filename) override
  {
    PRINT_NAMED_ERROR("LRRawPixelsClassifier.SerializeNotImplemented", "DeSerialize is not implemented for "
                                                                            "LRRawPixelsClassifier");
    return false;
  }

protected:

  bool Train(const cv::Mat& allInputs, const cv::Mat& allClasses, uint numberOfPositives) override;

  float _positiveClassWeight = 1.2; // This parameter should be overwritten by the Json config file
  float _trainingAlpha = 0.5;       // This parameter should be overwritten by the Json config file
  cv::Ptr<Anki::WeightedLogisticRegression> _logisticRegressor;
};

/****************************************************************
 *                     THRawPixelsClassifier               *
 ****************************************************************/

/*
 * Threshold based drivable surface classifier. During training this class builds a Gaussian Mixture Model out of the
 * data. Then during prediction the minimum Mahalanobis distance between the test point and the kernels is computed,
 * and the result is thresholded. The threshold is automatically found as a multiple of the median of the training data.
 */
class THRawPixelsClassifier : public GMMRawPixelsClassifier
{
public:
  explicit THRawPixelsClassifier(const Json::Value& config, const CozmoContext *context);

  using GMMRawPixelsClassifier::PredictClass;
  uchar PredictClass(const std::vector<FeatureType>& values) const override;

  bool TrainFromFiles(const char *positiveDataFileName, const char *negativeDataFileName) override;

  bool TrainFromFile(const char* positiveDataFilename);

  bool Serialize(const char *filename) override
  {
    PRINT_NAMED_ERROR("THRawPixelsClassifier.SerializeNotImplemented", "Serialize is not implemented for "
                                                                            "THRawPixelsClassifier");
    return false;
  }

  bool DeSerialize(const char *filename) override
  {
    PRINT_NAMED_ERROR("THRawPixelsClassifier.SerializeNotImplemented", "DeSerialize is not implemented for "
                                                                            "THRawPixelsClassifier");
    return false;
  }

protected:
  bool Train(const cv::Mat& allInputs, const cv::Mat&, uint) override;

  float FindThreshold(std::vector<float>& distances) const;

  float _threshold = -1.f;
  float _medianMultiplier = 5.0; // This value might be changed by the Json config
};

/****************************************************************
 *                     DTRawPixelsClassifier               *
 ****************************************************************/

/*
 * This class uses Decision Trees to classify drivable pixels. It does not build a Gaussian Mixture Model. It currently
 * is the fastest among the other classifiers and possibly the most accurate.
 */
class DTRawPixelsClassifier : public RawPixelsClassifier
{
public:
  explicit DTRawPixelsClassifier(const Json::Value& config, const CozmoContext *context,
                                 Anki::Vision::Profiler* profiler = nullptr);
  DTRawPixelsClassifier(const CozmoContext *context,
                        Anki::Vision::Profiler* profiler = nullptr);

  uchar PredictClass(const std::vector<FeatureType>& values) const override;
  using RawPixelsClassifier::PredictClass;
  std::vector<uchar> PredictClass(const Anki::Array2d<FeatureType>& features) const override;

  bool Serialize(const char *filename) override;

  bool DeSerialize(const char *filename) override;

protected:
  cv::Ptr<cv::ml::DTrees> _dtree;
  bool Train(const cv::Mat& allInputs, const cv::Mat& allClasses, uint numberOfPositives) override;

};

} // namespace Vector
} // namespace Anki


#endif //COZMO_DRIVINGSURFACECLASSIFIER_H
