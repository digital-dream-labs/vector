/**
 * File: rawPixelsClassifier.cpp
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

#include "rawPixelsClassifier.h"

#include "coretech/common/engine/math/logisticRegression.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "coretech/common/shared/array2d.h"
#include "coretech/vision/engine/profiler.h"
#include "engine/cozmoContext.h"
#include "util/fileUtils/fileUtils.h"

#include <fstream>
#include <typeinfo>

#define DEBUG_WRITE_DATA false
// support macro
#define GET_JSON_PARAMETER(__config, __paramname, __variable) \
  if (! JsonTools::GetValueOptional(__config, __paramname, __variable)) { \
    PRINT_NAMED_WARNING("RawPixelsClassifier.MissingJsonParameter", "Missing parameter %s", __paramname); \
  }

static const char* kLogChannelName = "VisionSystem";

namespace Anki {
namespace Vector {

namespace {

/*
 * Special case of Mahalanobis distance when the covariance matrix is diagonal
 */
float DiagonalMahalanobisDistance(const float *input, const double *means, const cv::Mat& cov, int dim = 3) {
  DEV_ASSERT(cov.type() == CV_64FC1, "DiagonalMahalanobisDistance.InvalidMatType");
  float sum = 0;
  for (int i = 0; i < dim; ++i) {
    const float dx = input[i] - float(means[i]);
    const float sigma = float(cov.at<double>(i,i)); // diagonal element, no need for fancy pointer math here
    sum += (dx * dx) / (sigma * sigma);
  }
  // no need to return sqrt here, we're only comparing magnitude
  return sum;
}

int AppendFileToMatrix(const char *filename, cv::Mat& mat) {

  std::ifstream file(filename);
  if (!file.is_open()) {
    PRINT_NAMED_ERROR("GMMRawPixelsClassifier.TrainFromFiles.ErrorOpeningFile", "Error while opening file %s",
                      filename);
    return -1;
  }

  std::string line;
  int numElements = 0;
  using SingleRow = cv::Mat_<float>;
  while (std::getline(file, line)) {
    if (line.size() == 0) {
      break; // end of the file?
    }
    std::istringstream stream(line);

    SingleRow row;
    double x;
    // assumes separator is white space
    while (stream >> x) {
      row.push_back(x);
    }
    SingleRow newrow = row.reshape(0, 1); // make the row flat
    mat.push_back(newrow);
    numElements++;
  }
  return numElements;

}

} // anonymous namespace

/****************************************************************
 *                     RawPixelsClassifier                      *
 ****************************************************************/

bool RawPixelsClassifier::Train(const OverheadMap::PixelSet& drivablePixels,
                                     const OverheadMap::PixelSet& nonDrivablePixels)
{
  // Build the training matrices for openCV
  const uint totalSize = uint(drivablePixels.size() + nonDrivablePixels.size());
  cv::Mat allInputs(totalSize, 3, CV_8UC1);
  cv::Mat allClasses(totalSize, 1, CV_8UC1);

  // iterators here make sense to modify two matrices in sequence
  {
    auto inputsIterator = allInputs.begin<cv::Vec3b>();
    auto outputsIterator = allClasses.begin<uchar>();

    // Positive class, drivable surface
    for (auto& pixel : drivablePixels) {
      (*inputsIterator)[0] = pixel.r();
      (*inputsIterator)[1] = pixel.g();
      (*inputsIterator)[2] = pixel.b();
      *outputsIterator = 1;

      ++inputsIterator;
      ++outputsIterator;
    }

    // Negative class, non drivable surface
    for (auto& pixel : nonDrivablePixels) {
      (*inputsIterator)[0] = pixel.r();
      (*inputsIterator)[1] = pixel.g();
      (*inputsIterator)[2] = pixel.b();
      *outputsIterator = 0;

      ++inputsIterator;
      ++outputsIterator;
    }
  }

  return Train(allInputs, allClasses, uint(drivablePixels.size()));
}

RawPixelsClassifier::RawPixelsClassifier(const CozmoContext *context, Anki::Vision::Profiler* profiler)
    : _context(context), _profiler(profiler)
{

}

void RawPixelsClassifier::GetTrainingData(cv::Mat& trainingSamples, cv::Mat& trainingLabels) const
{
  trainingSamples = _trainingSamples;
  trainingLabels = _trainingLabels;
}

std::vector<uchar> RawPixelsClassifier::PredictClass(const std::vector<std::vector<FeatureType>>& features) const
{
  std::vector<uchar> responses;
  responses.reserve(features.size());
  for (const auto& pixel : features) {

    uchar predictedClass = PredictClass(pixel);
    responses.push_back(predictedClass);
  }
  return responses;
}

std::vector<uchar> RawPixelsClassifier::PredictClass(const Array2d<RawPixelsClassifier::FeatureType>& features) const
{

  // In the most direct way this function just iterates over the elements in features and calls Predict()
  std::vector<uchar> responses(features.GetNumRows());
  for (uint i=0; i<features.GetNumRows(); i++) {
    const RawPixelsClassifier::FeatureType* row = features.GetRow(i);
    // builds the input vector. Unfortunately it has to copy
    const std::vector<RawPixelsClassifier::FeatureType> input(row, row + features.GetNumCols());
    uchar predictedClass = PredictClass(input);
    responses[i] = predictedClass;
  }
  return responses;
}

void RawPixelsClassifier::WriteMat(const cv::Mat& mat, const char *filename) const
{
  DEV_ASSERT(mat.channels() == 1, "RawPixelsClassifier.WriteMat.WrongNumberOfChannels");
  DEV_ASSERT((mat.type() == CV_32F) ||
             (mat.type() == CV_64F) ||
             (mat.type() == CV_8U), "RawPixelsClassifier.WriteMat.InvalidMatType");

  const std::string path = _context->GetDataPlatform()->pathToResource(Util::Data::Scope::Persistent,
                                                                       Util::FileUtils::FullFilePath({"vision",
                                                                                                      "overheadmap"}));
  if (!Util::FileUtils::CreateDirectory(path, false, true)) {
    PRINT_NAMED_ERROR("RawPixelsClassifier.WriteMat.DirectoryError", "Error while creating folder %s",
                      path.c_str());
    return;
  }

  PRINT_CH_INFO(kLogChannelName, "RawPixelsClassifier.WriteMat.PathInfo",
                "Saving the files to %s", path.c_str());

  const std::string fullPath = Util::FileUtils::FullFilePath({path, filename});
  std::ofstream outputFile(fullPath);
  if (! outputFile.is_open()) {
    PRINT_NAMED_ERROR("RawPixelsClassifier.WriteMat.FileNotOpen", "Error while opening file %s for writing",
                      fullPath.c_str());
    return;
  }
  switch (mat.type()) {
    case CV_32F:
      WriteMat<float>(mat, outputFile);
      break;
    case CV_64F:
      WriteMat<double>(mat, outputFile);
      break;
    case CV_8U:
      WriteMat<uchar>(mat, outputFile);
      break;
    default: // this will never happen
      DEV_ASSERT(false, "RawPixelsClassifier.WriteMat.WrongType");
      break;
  }

}

template<class T>
void RawPixelsClassifier::WriteMat(const cv::Mat& mat, std::ofstream& outputFile) const
{
  for (int i = 0; i < mat.rows; ++i) {
    const T* row = mat.ptr<T>(i);
    for (int j = 0; j < mat.cols; ++j) {
      outputFile<<row[j]<<" ";
    }
    outputFile << std::endl;
  }
}

bool RawPixelsClassifier::TrainFromFiles(const char *positiveDataFileName, const char *negativeDataFileName)
{
  cv::Mat inputElements;

  // positive elements
  const int numberOfPositives = AppendFileToMatrix(positiveDataFileName, inputElements);
  if (numberOfPositives < 0) {
    return false;
  }

  const int ret = AppendFileToMatrix(negativeDataFileName, inputElements);
  if (ret < 0) {
    return false;
  }

  // reshaping the matrix to be n x m with 1 channel
  inputElements = inputElements.reshape(1);

  cv::Mat classes;
  cv::vconcat(cv::Mat::ones(numberOfPositives, 1, CV_32FC1),
          cv::Mat::zeros(inputElements.rows - numberOfPositives, 1, CV_32FC1),
          classes);

  return Train(inputElements, classes, numberOfPositives);
}

void RawPixelsClassifier::SetTrainingData(const cv::Mat& trainingSamples, const cv::Mat& trainingLabels)
{
  _trainingSamples = trainingSamples;
  _trainingLabels = trainingLabels;
}

/****************************************************************
 *                     GMMDrivingSurfaceClassifier              *
 ****************************************************************/

GMMRawPixelsClassifier::GMMRawPixelsClassifier(const Json::Value& config, const CozmoContext *context)
  : RawPixelsClassifier(context)
{

  int numClusters = 5;
  GET_JSON_PARAMETER(config, "NumClusters", numClusters);

  _gmm = cv::ml::EM::create();
  _gmm->setClustersNumber(numClusters);
  _gmm->setCovarianceMatrixType(cv::ml::EM::COV_MAT_DIAGONAL);

}

bool GMMRawPixelsClassifier::TrainGMM(const cv::Mat& input)
{
  bool result;
  try {
    result = this->_gmm->trainEM(input);
  }
  catch (cv::Exception& e) {
    PRINT_NAMED_ERROR("GMMRawPixelsClassifier.Train.ErrorWhileTrainingEM", "%s", e.what());
    return false;
  }
  if (! result) {
    PRINT_NAMED_ERROR("GMMRawPixelsClassifier.Train.EMTRainingFail","");
    return false;
  }
  else {
    return true;
  }
}

std::vector<float> GMMRawPixelsClassifier::MinMahalanobisDistanceFromGMM(const cv::Mat& input, bool useWeight) const
{


  DEV_ASSERT(input.cols == 3, "GMMRawPixelsClassifier.MinMahalanobisDistanceFromGMM.Expected3Cols");

  DEV_ASSERT(input.channels() == 1, "Input matrix must have 1 channel");
  DEV_ASSERT(input.type() == CV_32F, "Input matrix must have float type");
  DEV_ASSERT(input.isContinuous(), "Input matrix must be continuous");

  // getting the data from the GMM
  std::vector<cv::Mat> covs;
  _gmm->getCovs(covs);

  const cv::Mat& weightsMat = _gmm->getWeights();
  DEV_ASSERT(weightsMat.type() == CV_64FC1, "Weight matrix has wrong type");
  const double* weights = weightsMat.ptr<double>(0); // single row (or column) matrix

  int nRows = input.rows;
  std::vector<float> result;
  result.reserve(nRows);

  for (int i = 0; i < nRows; ++i) {
    const float* inputRow = input.ptr<float>(i);

    // iterate over the components
    float minDistance = std::numeric_limits<float>::max();
    for (int k = 0; k < _gmm->getClustersNumber(); ++k) {
      const double w = weights[k];
      const cv::Mat meansMat = _gmm->getMeans();
      DEV_ASSERT(meansMat.type() == CV_64FC1, "GMMRawPixelsClassifier.MinMahalanobisDistanceFromGMM.WrongMatrixType");
      const double* means = _gmm->getMeans().ptr<double>(k);
      const cv::Mat& covariance = covs[k];
      DEV_ASSERT(covariance.type() == CV_64FC1, "GMMRawPixelsClassifier.MinMahalanobisDistanceFromGMM.WrongMatrixType");
      const int dims = 3;
      float dist = DiagonalMahalanobisDistance(inputRow, means, covariance, dims);
      if (useWeight) {
        dist = dist / w;
      }
      if ( dist < minDistance) {
        minDistance = dist;
      }
    }
    result.push_back(minDistance);
  }
  return result;
}


/****************************************************************
 *                     LRDrivingSurfaceClassifier               *
 ****************************************************************/

LRRawPixelsClassifier::LRRawPixelsClassifier(const Json::Value& config, const CozmoContext *context)
  : GMMRawPixelsClassifier(config, context)
{
  GET_JSON_PARAMETER(config, "TrainingAlpha", _trainingAlpha);
  GET_JSON_PARAMETER(config, "PositiveClassWeight", _positiveClassWeight);
  std::string regularization;
  int cvRegularization = cv::ml::LogisticRegression::REG_DISABLE;
  GET_JSON_PARAMETER(config, "RegularizationType", regularization);
  if (! regularization.empty()) {
    if (regularization == "L1") {
      cvRegularization = cv::ml::LogisticRegression::REG_L1;
    }
    else if (regularization == "L2") {
      cvRegularization = cv::ml::LogisticRegression::REG_L2;
    }
    else if (regularization == "Disable") {
      cvRegularization = cv::ml::LogisticRegression::REG_DISABLE;
    }
    else {
      PRINT_NAMED_WARNING("LRRawPixelsClassifier.WrongJsonParameter", "Regularization value is unknown: %s. Valid"
          " values are (L1, L2, Disable)", regularization.c_str());
    }
  }
  uint numIterations = 1000;
  GET_JSON_PARAMETER(config, "NumIterations", numIterations);

  _logisticRegressor = cv::makePtr<WeightedLogisticRegression>();
  _logisticRegressor->setIterations(numIterations);
  _logisticRegressor->setRegularization(cvRegularization);
  _logisticRegressor->setTrainMethod(cv::ml::LogisticRegression::BATCH);

}

bool LRRawPixelsClassifier::Train(const cv::Mat& allInputs, const cv::Mat& allClasses, uint numberOfPositives)
{
  DEV_ASSERT(allInputs.cols == 3, "Input matrix must have 3 columns");
  DEV_ASSERT(allInputs.channels() == 1, "Input matrix must have 1 channel");
  DEV_ASSERT(allInputs.type() == CV_32F, "Input matrix must have CV_32F type");
  DEV_ASSERT(allInputs.isContinuous(), "Input matrix must be continuous");

  DEV_ASSERT(allClasses.cols == 1, "Classes matrix must have 1 column");
  DEV_ASSERT(allClasses.channels() == 1, "Classes matrix must have 1 channel");
  DEV_ASSERT(allClasses.type() == CV_32F, "Classes matrix must have CV_32F type");
  DEV_ASSERT(allClasses.isContinuous(), "Classes matrix must be continuous");

  DEV_ASSERT(allInputs.rows == allClasses.rows, "Input and Classes matrix must have the same size");

  _trainingSamples = allInputs;
  _trainingLabels = allClasses;

  // Train the Gaussian Mixture Model
  {
    // only positive examples
    cv::Mat gmmInput = allInputs.rowRange(0, numberOfPositives);
    const bool result = TrainGMM(gmmInput);
    if (! result) {
      return false;
    }
  }

  {
    const cv::Mat& weights = _gmm->getWeights();
    DEV_ASSERT(weights.rows == 1 || weights.cols == 1, "Wrong weights size!");
  }

  // Train Logistic Regression
  {
    const uint totalSize = allInputs.rows;
    // set the learning rate here as a multiple of the number of elements. OpenCV quirkiness
    _logisticRegressor->setLearningRate(_trainingAlpha * totalSize);
    const std::vector<float> minDistances = this->MinMahalanobisDistanceFromGMM(allInputs, true);
    const cv::Mat minDistancesMat = cv::Mat(minDistances);

    if (DEBUG_WRITE_DATA) {
      WriteMat(minDistancesMat, "minDistancesMat.txt");
      WriteMat(allClasses, "allClasses.txt");
    }

    // create the weights vector
    cv::Mat sampleWeights(totalSize, 1, CV_32F);
    {
      float* weightsRow = sampleWeights.ptr<float>(0);
      for (uint i = 0; i < numberOfPositives; ++i) {
        weightsRow[i] = this->_positiveClassWeight;
      }
      for (uint i = numberOfPositives; i < totalSize; ++i) {
        weightsRow[i] = 1.0;
      }
    }

    const cv::Ptr<cv::ml::TrainData> trainingData = cv::ml::TrainData::create(minDistancesMat,
                                                                              cv::ml::SampleTypes::ROW_SAMPLE,
                                                                              allClasses,
                                                                              cv::noArray(),
                                                                              cv::noArray(),
                                                                              sampleWeights);

    bool result;
    try {
      result = this->_logisticRegressor->train(trainingData);
    }
    catch (cv::Exception& e) {
      PRINT_NAMED_ERROR("LRRawPixelsClassifier.Train.ErrorWhileTrainingLogistic", "%s", e.what());
      return false;
    }
    if (! result) {
      PRINT_NAMED_ERROR("LRRawPixelsClassifier.Train.LogisticTrainingFail","Result from training is false!");
      return false;
    }
  }

  return true;
}

uchar LRRawPixelsClassifier::PredictClass(const std::vector<FeatureType>& values) const
{
  //TODO Assuming a single pixel here
  DEV_ASSERT(values.size() == 3, "LRRawPixelsClassifier.PredictClass.WrongInputSize");

  // Step 1: get the GMM response
  const cv::Mat pixelMat = (cv::Mat_<float>(1,3) << values[0], values[1], values[2]);

  // Step 2: calculate the Mahalanobis distance
  const std::vector<float> minDistances = this->MinMahalanobisDistanceFromGMM(pixelMat);
  const cv::Mat minDistancesMat = cv::Mat(minDistances);

  // Step 3: response of Logistic Regression
  std::vector<float> result;
  _logisticRegressor->predict(minDistancesMat, result);

  DEV_ASSERT(result.size() == 1, "LRRawPixelsClassifier.PredictClass.EmptyResultVector");
  return uchar(result[0]);
}

/****************************************************************
 *                     THRawPixelsClassifier                    *
 ****************************************************************/

uchar THRawPixelsClassifier::PredictClass(const std::vector<FeatureType>& values) const
{
  //TODO Assuming a single pixel here
  DEV_ASSERT(values.size() == 3, "THRawPixelsClassifier.PredictClass.WrongInputSize");

  // Step 1: get the GMM response
  const cv::Mat pixelMat = (cv::Mat_<float>(1,3) << values[0], values[1], values[2]);

  // Step 2: calculate the Mahalanobis distance
  const std::vector<float> minDistances = this->MinMahalanobisDistanceFromGMM(pixelMat);

  // There should be only one element
  DEV_ASSERT(minDistances.size() == 1, "THRawPixelsClassifier.PredictClass.WrongMinDistancesSize");
  const float minDistance = minDistances[0];

  return (minDistance <= _threshold) ? uchar(1) : uchar(0);
}

bool THRawPixelsClassifier::Train(const cv::Mat& allInputs, const cv::Mat&, uint)
{
  DEV_ASSERT(allInputs.cols == 3, "Input matrix must have 3 columns");
  DEV_ASSERT(allInputs.channels() == 1, "Input matrix must have 1 channel");
  DEV_ASSERT(allInputs.type() == CV_32F, "Input matrix must have CV_32F type");
  DEV_ASSERT(allInputs.isContinuous(), "Input matrix must be continuous");

  // Train the Gaussian Mixture Model
  _trainingSamples = allInputs;
  bool result = TrainGMM(allInputs);
  if (! result) {
    return false;
  }

  // find the threshold
  std::vector<float> minDistances = this->MinMahalanobisDistanceFromGMM(allInputs, true);
  _threshold = FindThreshold(minDistances);

  PRINT_CH_DEBUG(kLogChannelName, "THRawPixelsClassifier.Train.Threshold", "Found a threshold of %f", _threshold);
  return true;
}

THRawPixelsClassifier::THRawPixelsClassifier(const Json::Value& config, const CozmoContext *context)
    : GMMRawPixelsClassifier(config, context)
{
  GET_JSON_PARAMETER(config, "MedianMultiplier", _medianMultiplier);
}

bool THRawPixelsClassifier::TrainFromFile(const char *positiveDataFilename)
{
  cv::Mat inputElements;
  const int numberOfElements = AppendFileToMatrix(positiveDataFilename, inputElements);
  if (numberOfElements < 0) {
    return false;
  }

  // reshaping the matrix to be n x 3 with 1 channel
  inputElements = inputElements.reshape(1);

  return Train(inputElements, cv::Mat(), numberOfElements);
}

float THRawPixelsClassifier::FindThreshold(std::vector<float>& distances) const
{
  // find the median element and set the threshold to be a multiple of that number
  size_t halfSize = distances.size() / 2;
  std::nth_element(distances.begin(), distances.begin() + halfSize, distances.end());
  float median = distances[halfSize];

  return median * _medianMultiplier;
}

bool THRawPixelsClassifier::TrainFromFiles(const char*, const char*)
{
  PRINT_NAMED_ERROR("THRawPixelsClassifier.TrainFromFiles.NotImplemented",
                    "THRawPixelsClassifier does not support training from positive and negative elements "
                    "(only positive)");
  return false;
}

/****************************************************************
 *                     DTDrivingSurfaceClassifier               *
 ****************************************************************/

DTRawPixelsClassifier::DTRawPixelsClassifier(const Json::Value& config, const CozmoContext *context,
                                             Anki::Vision::Profiler* profiler)
  : RawPixelsClassifier(context, profiler)
{
  _dtree = cv::ml::DTrees::create();

  int maxdepth = 10;
  GET_JSON_PARAMETER(config, "MaxDepth", maxdepth);
  int minSampleCount = 2;
  GET_JSON_PARAMETER(config, "MinSampleCount", minSampleCount);
  bool truncatePrunedTree = false;
  GET_JSON_PARAMETER(config, "TruncatePrunedTree", truncatePrunedTree)
  bool use1SERule = true;
  GET_JSON_PARAMETER(config, "Use1SERule", use1SERule);
  float positiveWeight = 1.0f;
  GET_JSON_PARAMETER(config, "PositiveWeight", positiveWeight);

  _dtree->setMaxDepth(maxdepth);
  _dtree->setMinSampleCount(minSampleCount);
  _dtree->setTruncatePrunedTree(truncatePrunedTree);
  // prior
  const cv::Mat prior = (cv::Mat_<float>(1,2) << 1.0, positiveWeight);
  _dtree->setPriors(prior);

  // fixed parameters
  _dtree->setUseSurrogates(false);
  _dtree->setCVFolds(0);
  _dtree->setMaxCategories(2);

}

DTRawPixelsClassifier::DTRawPixelsClassifier(const CozmoContext *context,
                                             Anki::Vision::Profiler* profiler)
    : RawPixelsClassifier(context, profiler)
{
  // create an empty dtree
  _dtree = cv::ml::DTrees::create();
}

uchar DTRawPixelsClassifier::PredictClass(const std::vector<FeatureType>& values) const
{

  DEV_ASSERT(values.size() == _dtree->getVarCount(), "DTRawPixelsClassifier.PredictClass.WrongInputSize");

  //DTree requires Mat_<float> as input

  cv::Mat_<float> inputRow;
  if (typeid(FeatureType) == typeid(float)) {
    inputRow = cv::Mat_<float>(values).reshape(1, 1); // make it a single row
  }
  else {
    // Need to copy
    // TODO can the Mat_ constructor just do this?

    inputRow = cv::Mat_<float>(1, int(values.size()));
    auto rowItr = inputRow.begin();
    auto valuesItr = values.begin();
    // copying all the elements into a mat
    for (; valuesItr != values.end(); valuesItr++, rowItr++) {
      *rowItr = float(*valuesItr);
    }
  }

  std::vector<float> result;
  _dtree->predict(inputRow, result);

  DEV_ASSERT(result.size() == 1, "DTRawPixelsClassifier.PredictClass.EmptyResultVector");
  return uchar(result[0]);
}

bool DTRawPixelsClassifier::Train(const cv::Mat& allInputs, const cv::Mat& allClasses, uint)
{

  DEV_ASSERT(allInputs.cols % 3 == 0, "Input matrix must have a multiple of 3 columns");
  DEV_ASSERT(allInputs.channels() == 1, "Input matrix must have 1 channel");
  DEV_ASSERT(allInputs.type() == CV_32F, "Input matrix must have CV_32F type");
  DEV_ASSERT(allInputs.isContinuous(), "Input matrix must be continuous");

  DEV_ASSERT(allClasses.cols == 1, "Classes matrix must have 1 column");
  DEV_ASSERT(allClasses.channels() == 1, "Classes matrix must have 1 channel");
  DEV_ASSERT(allClasses.type() == CV_32F, "Classes matrix must have CV_32F type");
  DEV_ASSERT(allClasses.isContinuous(), "Classes matrix must be continuous");

  DEV_ASSERT(allInputs.rows == allClasses.rows, "Input and Classes matrix must have the same size");

  // we actually need the data to be of type CV_32S here :(
  allClasses.convertTo(_trainingLabels, CV_32S);

  _trainingSamples = allInputs;

  // create the training data structure
  const cv::Ptr<cv::ml::TrainData> trainingData = cv::ml::TrainData::create(allInputs,
                                                                            cv::ml::SampleTypes::ROW_SAMPLE,
                                                                            _trainingLabels
                                                                            );
  DEV_ASSERT(trainingData->getResponseType() == cv::ml::VAR_CATEGORICAL,
             "DTRawPixelsClassifier.Train.WrongTrainingDataType");

  bool result;
  try {
    result = this->_dtree->train(trainingData);
  }
  catch (cv::Exception& e) {
    PRINT_NAMED_ERROR("DTRawPixelsClassifier.Train.ErrorWhileTrainingLogistic", "%s", e.what());
    return false;
  }
  if (! result) {
    PRINT_NAMED_ERROR("DTRawPixelsClassifier.Train.LogisticTrainingFail","Result from training is false!");
    return false;
  }

  return true;
}

bool DTRawPixelsClassifier::Serialize(const char *filename)
{

  _dtree->save(filename);

  return true;
}

bool DTRawPixelsClassifier::DeSerialize(const char *filename)
{

  if (!Anki::Util::FileUtils::FileExists(filename)) {
    PRINT_NAMED_ERROR("DTRawPixelsClassifier.DeSerialize.FileDoesntExist", "Error: file %s doesn't exist!",
                      filename);
  }

  _dtree = cv::ml::DTrees::load(filename);

  if (_dtree->empty()) {
    PRINT_NAMED_ERROR("DTRawPixelsClassifier.DeSerialize.ErrorWhileDeserializing", "Error: dtree is empty after"
        "loading from %s", filename);
    return false;
  }

  PRINT_CH_DEBUG(kLogChannelName,"DTRawPixelsClassifier.DeSerialize.Success", "Successfully loaded file %s",
                 filename);
  return true;

}

std::vector<uchar> DTRawPixelsClassifier::PredictClass(const Array2d<RawPixelsClassifier::FeatureType>& features) const
{
  DEV_ASSERT(features.GetNumCols() == _dtree->getVarCount(), "DTRawPixelsClassifier.PredictClass.WrongInputSize");

  cv::Mat cvFeatures = features.get_CvMat_(); // intentionally not taking const reference here, might need to re-assign

  //DTree requires Mat_<float> as input
  if (typeid(RawPixelsClassifier::FeatureType) != typeid(float)) {
    cv::Mat converted;
    cvFeatures.convertTo(converted, CV_32F);
    cvFeatures = converted;
  }
  cv::Mat output;
  const auto& tree = *_dtree;
  tree.predict(cvFeatures, output);

  // now convert to a std::vector uchar scaling 1 to 255
  std::vector<uchar> toRet;
  output.convertTo(toRet, CV_8U, 255);
  return toRet;
}


} // namespace Vector
} // namespace Anki
