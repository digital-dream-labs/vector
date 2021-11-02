/**
 * File: GroundPlaneClassifier.cpp
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


#include "groundPlaneClassifier.h"

#include "coretech/common/engine/math/logisticRegression.h" // TODO this is temporary only for calculateError
#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "engine/cozmoContext.h"
#include "engine/overheadEdge.h"
#include "engine/vision/groundPlaneROI.h"
#include "util/fileUtils/fileUtils.h"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#define DEBUG_DISPLAY_IMAGES false

namespace Anki {
namespace Vector {

/****************************************************************
 *                     Helper Functions                         *
 ****************************************************************/

void ClassifyImage(const RawPixelsClassifier& clf, const Anki::Vector::IFeaturesExtractor& extractor,
                   const Vision::ImageRGB& image, Vision::Image& outputMask)
{
  s32 nrows = image.GetNumRows();
  s32 ncols = image.GetNumCols();
  DEV_ASSERT(outputMask.GetNumRows() == nrows && outputMask.GetNumCols() == ncols,
             "ClassifyImage.ResultArraySizeMismatch");

  // Get the features and pass them to the classifier
  auto featuresList = extractor.Extract(image);
  std::vector<uchar> classes = clf.PredictClass(featuresList);

  // convert into a binary image
  //need to copy data here, the vector will go out of scope
  cv::Mat(classes, true).reshape(1, image.GetNumRows()).copyTo(outputMask.get_CvMat_());

  // That's all folks!
}

/****************************************************************
 *                    Ground Plane Classifier                   *
 ****************************************************************/

GroundPlaneClassifier::GroundPlaneClassifier(const Json::Value& config, const CozmoContext *context)
  : _context(context)
  , _initialized(false)
  , _profiler("GroundPlaneClassifier")
{

  _profiler.SetPrintFrequency(2000);

  DEV_ASSERT(context != nullptr, "GroundPlaneClassifier.ContextCantBeNULL");


  // TODO Classifier and extractor (with their parameters) should be passed at config time!
  _classifier.reset(new DTRawPixelsClassifier(config, context, &_profiler));
  _extractor.reset(new MeanFeaturesExtractor(1));

  // Train or load serialized file?
  bool onTheFlyTrain;
  if (!JsonTools::GetValueOptional(config, "OnTheFlyTrain", onTheFlyTrain)) {
    PRINT_NAMED_ERROR("GroundPlaneClassifier.MissingOnTheFlyTrain","Variable OnTheFlyTrain has to be specified!");
    return;
  }
  std::string path;
  if (!JsonTools::GetValueOptional(config, "FileOrDirName", path)) {
    PRINT_NAMED_ERROR("GroundPlaneClassifier.MissingFileOrDirName","Variable FileOrDirName has to be specified!");
    return;
  }
  const std::string fullpath = context->GetDataPlatform()->pathToResource(Anki::Util::Data::Scope::Resources,
                                                                          path);
  PRINT_CH_DEBUG("VisionSystem", "GroundPlaneClassifier.FullPathName", "The full path is %s", fullpath.c_str());

  if (onTheFlyTrain) { // filename is the folder where positivesPixels.txt and negativePixels.txt are stored
    PRINT_CH_DEBUG("VisionSystem", "GroundPlaneClassifier.TrainingOnTheFly","Training the classifier");
    TrainClassifier(fullpath);
  }
  else {
    if (!LoadClassifier(fullpath)) {
      PRINT_CH_DEBUG("VisionSystem", "GroundPlaneClassifier.LoadingSavedClassifier","Error while loading classifier!");
      return;
    }
  }
  _initialized = true;

}

Result GroundPlaneClassifier::Update(const Vision::ImageRGB& image, const VisionPoseData& poseData,
                                     Vision::DebugImageList<Vision::CompressedImage>& debugImages,
                                     std::list<OverheadEdgeFrame>& outEdges)
{

  auto tictoc = _profiler.TicToc("GroundPlaneClassifier.Update");
  // nothing to do here if there's no ground plane visible
  if (!poseData.groundPlaneVisible) {
    PRINT_CH_DEBUG("VisionSystem", "GroundPlaneClassifier.Update.GroundPlane", "Ground plane is not visible");
    return RESULT_OK;
  }
  if (!IsInitialized()) {
    PRINT_NAMED_ERROR("GroundPlaneClassifier.NotInitialized", "Ground Plane Classifier is not initialized");
    return RESULT_FAIL;
  }

  PRINT_CH_DEBUG("VisionSystem", "GroundPlaneClassifier.Update.Starting","");

  // STEP 1: Obtain the overhead ground plane image
  GroundPlaneROI groundPlaneROI;
  const Matrix_3x3f& H = poseData.groundPlaneHomography;
  const Vision::ImageRGB groundPlaneImage = groundPlaneROI.GetOverheadImage(image, H);

  // STEP 2: Classify the overhead image
  Vision::Image rawClassifiedImage(groundPlaneImage.GetNumRows(), groundPlaneImage.GetNumCols(), u8(0));
  _profiler.Tic("GroundPlaneClassifier.ClassifyImage");
  ClassifyImage(*_classifier.get(), *_extractor.get(), groundPlaneImage, rawClassifiedImage);
  _profiler.Toc("GroundPlaneClassifier.ClassifyImage");

  // STEP 2.5: Postprocess the classified mask (e.g. smoothing, noise removal)
  const Vision::Image classifiedMask = ProcessClassifiedImage(rawClassifiedImage);

  // STEP 3: Find leading edge in the classified mask (i.e. closest edge of obstacle to robot)
  OverheadEdgeFrame edgeFrame;
  OverheadEdgeChainVector& candidateChains = edgeFrame.chains;
  OverheadEdgePoint edgePoint;
  const Vision::Image& overheadMask = groundPlaneROI.GetOverheadMask();
  for(s32 i=0; i<classifiedMask.GetNumRows(); ++i)
  {
    const u8* overheadMask_i   = overheadMask.GetRow(i);
    const u8* classifiedMask_i = classifiedMask.GetRow(i);

    // Walk along the row and look for the first transition from not
    const Point2f& overheadOrigin = groundPlaneROI.GetOverheadImageOrigin();
    for(s32 j=1; j<classifiedMask.GetNumCols(); ++j)
    {
      // *Both* pixels must be *in* the overhead mask so we don't get leading edges of the boundary of
      // the ground plane quad itself!
      const bool bothInMask = ((overheadMask_i[j-1] > 0) && (overheadMask_i[j] > 0));
      if(bothInMask)
      {
        // To be a leading edge of an obstacle according to the classifier, first pixel should be "on" (drivable) and
        // second pixel should be "off" (not drivable)
        const bool isLeadingEdge = ((classifiedMask_i[j-1]>0) && (classifiedMask_i[j]==0));
        if(isLeadingEdge)
        {
          // Note that rows in ground plane image are robot y, and cols are robot x.
          // Just need to offset them to the right origin.
          edgePoint.position.x() = static_cast<f32>(j) + overheadOrigin.x();
          edgePoint.position.y() = static_cast<f32>(i) + overheadOrigin.y();
          edgePoint.gradient = 0.f; // TODO: Do we need the gradient for anything?

          candidateChains.AddEdgePoint(edgePoint, true);

          // No need to keep looking at this row as soon as a leading edge is found
          break;
        }
      }
    }
  }

  // TODO this can be a parameter
  const u32 kMinChainLength_mm = 5;
  candidateChains.RemoveChainsShorterThan(kMinChainLength_mm);
  // TODO add other post-processing step (e.g. ray trace from the robot to remove obstacles "behind" others

  if(DEBUG_DISPLAY_IMAGES)
  {
    debugImages.emplace_back("OverheadImage", groundPlaneImage);

    Vision::ImageRGB leadingEdgeDisp(classifiedMask);

    static const std::vector<ColorRGBA> lineColorList = {
      NamedColors::RED, NamedColors::GREEN, NamedColors::BLUE,
      NamedColors::ORANGE, NamedColors::CYAN, NamedColors::YELLOW,
    };
    auto color = lineColorList.begin();

    const Point2f& overheadOrigin = groundPlaneROI.GetOverheadImageOrigin();

    for (const auto &chain : candidateChains.GetVector())
    {
      // Draw line segments between all pairs of points in this chain
      Anki::Point2f startPoint(chain.points[0].position);
      startPoint -= overheadOrigin;

      for (s32 i = 1; i < chain.points.size(); ++i) {
        Anki::Point2f endPoint(chain.points[i].position);
        endPoint -= overheadOrigin;

        leadingEdgeDisp.DrawLine(startPoint, endPoint, *color, 3);
        std::swap(endPoint, startPoint);
      }

      // Switch colors for next segment
      ++color;
      if (color == lineColorList.end()) {
        color = lineColorList.begin();
      }
    }

    debugImages.emplace_back("LeadingEdges", leadingEdgeDisp);

    // Draw Ground plane on the camera image and display it
    Vision::ImageRGB toDisplay;
    image.CopyTo(toDisplay);
    Quad2f quad;
    if (poseData.groundPlaneVisible) {
      groundPlaneROI.GetImageQuad(H,
                                  toDisplay.GetNumCols(),
                                  toDisplay.GetNumRows(),
                                  quad);

      toDisplay.DrawQuad(quad, NamedColors::WHITE, 3);
    }
    // still send an image with no ground plane drawn on it. Courtesy of Al (see VIC-793)
    debugImages.emplace_back("GroundQuadImage", toDisplay);
  }

  // Actually return the resulting edges in the provided list
  outEdges.emplace_back(std::move(edgeFrame));

  PRINT_CH_DEBUG("VisionSystem", "GroundPlaneClassifier.Update.Stopping","");
  return RESULT_OK;
}

Vision::Image GroundPlaneClassifier::ProcessClassifiedImage(const Vision::Image& binaryImage) const
{

  cv::Mat_<u8> cvImage = binaryImage.get_CvMat_();

  // TODO these values should be parameters
  cv::morphologyEx(cvImage, cvImage,
                   cv::MORPH_CLOSE,
                   cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5,5)),
                   cv::Point(-1, -1),
                   2
  );

  return Vision::Image(cvImage);
}

void GroundPlaneClassifier::TrainClassifier(const std::string& path)
{
  const std::string positivePath = Anki::Util::FileUtils::FullFilePath({path, "positivePixels.txt"});
  const std::string negativePath = Anki::Util::FileUtils::FullFilePath({path, "negativePixels.txt"});

  bool result = _classifier->TrainFromFiles(positivePath.c_str(), negativePath.c_str());

  DEV_ASSERT(result, "GroundPlaneClassifier.TrainingFromFiles");

  // Error on whole training set
  {
    cv::Mat trainingSamples, trainingLabels;
    _classifier->GetTrainingData(trainingSamples, trainingLabels);

    // building a std::vector<std::vector<u8>>
    std::vector<std::vector<RawPixelsClassifier::FeatureType>> values;
    values.reserve(trainingSamples.rows);
    CVMatToVector<float>(trainingSamples, values);

    // calculate error
    // this adds some overhead but we're not planning to train a classifier on the robot
    const std::vector<u8> responses = _classifier->PredictClass(values);
    const cv::Mat responsesMat(responses);

    PRINT_CH_DEBUG("VisionSystem", "GroundPlaneClassifier.Train.ErrorLevel",
                   "Error after training is: %f", Anki::calculateError(responsesMat, trainingLabels));
  }
  _initialized = true;

}

bool GroundPlaneClassifier::LoadClassifier(const std::string& filename)
{
  if (!_classifier->DeSerialize(filename.c_str())) {
    PRINT_NAMED_ERROR("GroundPlaneClassifier.LoadClassifier.ErrorWhileLoading", "Error while loading %s",
                      filename.c_str());
    _initialized = false;
    return false;
  }
  else {
    _initialized = true;
    return true;
  }
}


/****************************************************************
 *                     Features Extractors                      *
 ****************************************************************/

std::vector<RawPixelsClassifier::FeatureType>
MeanFeaturesExtractor::Extract(const Vision::ImageRGB& image, int row, int col) const
{

  // TODO don't use this function, use the overloaded version below that is way more efficient

  // The main work here is because this function will be called several times. The first time calculate the mean image
  // and cache it, the other times just access the elements directly.

  const cv::Mat& cvImage = image.get_CvMat_();
  // if the image has changed
  if (cvImage.data != _prevImageData) {
    auto tictoc = _profiler.TicToc("MeanFeaturesExtractor.Extract.CreateMeanMatrix");
    _prevImageData = cvImage.data;

    const uint kernelSize = 2*_padding + 1;
    cv::boxFilter(cvImage, _meanImage, CV_32F, cv::Size(kernelSize, kernelSize),
                                                        cv::Point(-1, -1), true,
                                                        cv::BORDER_REPLICATE);
    DEV_ASSERT(_meanImage.type() == CV_32FC3, "MeanFeaturesExtractor.Extract.WrongOutputType");
    _meanMatIterator = _meanImage.begin<cv::Vec3f>();
  }

  // Need to make sure that we are scanning the image the right way
  {
    const cv::Point iteratorPoint = _meanMatIterator.pos();
    DEV_ASSERT(iteratorPoint.x == col, "MeanFeaturesExtractor.Extract.WrongIteratorX");
    DEV_ASSERT(iteratorPoint.y == row, "MeanFeaturesExtractor.Extract.WrongIteratorY");
  }

  cv::Vec3f& vec = *_meanMatIterator;
  std::vector<RawPixelsClassifier::FeatureType> toRet = {RawPixelsClassifier::FeatureType(vec[0]),
                                                         RawPixelsClassifier::FeatureType(vec[1]),
                                                         RawPixelsClassifier::FeatureType(vec[2])};
  _meanMatIterator++;
  return toRet;

}

Array2d<RawPixelsClassifier::FeatureType> MeanFeaturesExtractor::Extract(const Vision::ImageRGB& image) const
{
  const cv::Mat& cvImage = image.get_CvMat_();

  // Calculate mean over all image
  cv::Mat meanImage;
  const uint kernelSize = 2*_padding + 1;
  // NOTE: not using optimized Image::BoxFilter because we use float data here, not uint8
  // TODO use NEON optimized boxFilter when available for float data
  cv::boxFilter(cvImage, meanImage, CV_32F, cv::Size(kernelSize, kernelSize),
                cv::Point(-1, -1), true,
                cv::BORDER_REPLICATE);

  DEV_ASSERT(meanImage.type() == CV_32FC3, "MeanFeaturesExtractor.Extract.WrongOutputType");

  // reshaping the matrix to be n x 3 with 1 channel
  const int newNumberOfRows = meanImage.rows * meanImage.cols;
  cv::Mat listOfRows = meanImage.reshape(1, newNumberOfRows); // non-const, might need to be changed for the type
  DEV_ASSERT(listOfRows.type() == CV_32FC1, "MeanFeaturesExtractor.Extract.WrongReshape");
  DEV_ASSERT(listOfRows.cols == 3, "MeanFeaturesExtractor.Extract.Not3Columns");

  // convert to FeatureType if necessary
  if (typeid(RawPixelsClassifier::FeatureType) != typeid(float)) {
    cv::Mat converted;
    listOfRows.convertTo(converted, cv::DataType<RawPixelsClassifier::FeatureType>::type);
    listOfRows = converted;
  }

  return Array2d<RawPixelsClassifier::FeatureType>(listOfRows);
}

std::vector<RawPixelsClassifier::FeatureType>
SinglePixelFeaturesExtraction::Extract(const Vision::ImageRGB& image, int row, int col) const
{
  const Vision::PixelRGB& pixel = image(row, col);
  const std::vector<RawPixelsClassifier::FeatureType> toRet{RawPixelsClassifier::FeatureType(pixel.r()),
                                                            RawPixelsClassifier::FeatureType(pixel.g()),
                                                            RawPixelsClassifier::FeatureType(pixel.b())};
  return toRet;
}

Array2d<RawPixelsClassifier::FeatureType> SinglePixelFeaturesExtraction::Extract(const Vision::ImageRGB& image) const
{

  // reshape the original image to have 1 channel and size n x 3
  // conversion to RawPixelsClassifier::FeatureType will be handled by the cv::Mat_ constructor
  cv::Mat_<RawPixelsClassifier::FeatureType> toRet = image.get_CvMat_().reshape(1,
                                                                                image.GetNumRows() * image.GetNumCols());
  DEV_ASSERT(toRet.rows == image.GetNumRows() * image.GetNumCols(),
             "SinglePixelFeaturesExtraction.Extract.WrongNumberOfRows");
  DEV_ASSERT(toRet.cols == 3,
             "SinglePixelFeaturesExtraction.Extract.WrongNumberOfCols");

  return Array2d<RawPixelsClassifier::FeatureType>(toRet);

}
} // namespace Vector
} // namespace Anki
