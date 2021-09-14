## Neural Net Runner / Model

### Quick Summary

* Forward inference through neural nets runs asynchronously within the VisionSystem, via a `std::future`
  - NeuralNetRunner is a sub-component of the VisionSystem which communicates with the underlying INeuralNetModel and abstracts away the additional level of asynchrony from the engine's point of view.
  - We have wrappers for OpenCV's DNN module (deprecated), TensorFlow, and TensorFlow Lite (TFLite). TFLite is the current default.
  - We have our own ["private fork" of TensorFlow](https://github.com/anki/tensorflow), which has an `anki` branch with an `anki` subdirectory containing our special build scripts (e.g. for building a `vicos`-compatible version of TensorFlow / TFLite).
* Parameters of the underlying network model are configured in their own section of [`vision_config.json`](/resources/config/engine/vision_config.json)
* NeuralNets return instances of SalientPoints, which are centroids of "interesting" things in an image, with some associated information (like confidence, shape/bounding box, and type). These are stored in the SalientPointComponent in the engine, for the behaviors to check and act upon.

---

### Details

In the VisionSystem, we have a NeuralNetRunner, which wraps the underlying system actually doing forward inference with the neural net. This means that the rest of the engine / VisionSystem does not need to know whether the neural net is running in a separate process (which we did for awhile, as `vic-neuralnets`), as a thread in the same process (which we are back to using), or even offboard in the cloud or on a laptop (which we may do in the future). This class handles the additional asynchrony for us (using a `std::future`). 

A side effect of the additional asynchrony here is that the NeuralNetRunner is handled slightly differently by the VisionSystem versus other vision sub-components. SalientPoints may appear in a VisionProcessingResult for an image well after the one used for forward inference through the network. Thus the SalientPoints' timestamps may not match the VisionProcessingResult's timestamp. I.e., the SalientPoints may not be from the same image as markers, faces, etc, returned in the same VisionProcessingResult.

The NeuralNetRunner has a model, which implements an INeuralNetModel interface. Currently, we have three implementations: one for "full" [TensorFlow](https://www.tensorflow.org/) models, one for [TensorFlow Lite](https://www.tensorflow.org/mobile/tflite/) models, and one for "offboard" models. The TFLite model is far better optimized and supports quantization, and is the one we use in production for on-board neural nets.

Note that we also have an INeuralNetMain interface, which we used to use both for `vic-neuralnets`, which ran as a separate process on the robot, and `webotsCtrlNeuralnets`, which was the corresponding Webots controller for running neural nets in simulation. That class handles the "IPC", i.e. polling for new image files to process and writing SalientPoint outputs from the neural net model to disk for the NeuralNetRunner in the VisionSystem to pick up. Now that we run models back within the engine process again, this is not currently used in production code, but the interface still exists within the `cti_neuralnets` library.

See also the [Rock, Paper Scissors project from the 2018 R&D Sprint](https://ankiinc.atlassian.net/wiki/spaces/RND/pages/197591128/Rock+Paper+Scissors+using+Deep+Learning), and the associated slides for more details.

### Model Parameters

In the "NeuralNets" field of [`vision_config.json`](/resources/config/engine/vision_config.json), you can specify several parameters of the model you want to load. See NeuralNetParams for additional info beyond the following.

* `modelType` - Maps to which implementation of INeuralNetModel the runner will use. Currently we only have "TFLite" models in place, but this will allow us to have some models running "offboard" in the future as well.
* `graphFile` - The graph representing the model you want to load, assumed to be in `resources/config/engine/vision/dnn_models` (see also the next section about model storage).
  - `<filename.{pb|tflite}>` will load a TensorFlow (Lite) model stored in a Google protobuf file
  - `<modelname>` (without extension) assumes you are specifying a Caffe model and will load the model from `<modelname>.prototxt` and `<modelname>.caffemodel`, which must _both_ exist.  
* `labelsFile` - The filename containing the labels for translating numeric values returned by the classifier/detector into string names: one label on each line. Assumed to be in the same directory as the model file. Note that the the "background" class is assumed to be the zero label and is not required in the file. So there are actually N-1 rows. So for a binary classifier (Class A vs. Background), you'd just have one row: "Class A".
* `inputWidth` and `inputHeight` - The resolution at which to process the images (should generally match what the model expects). 
* `outputType` - What kind of output the network produces. Four output types are currently supported:
  * "Classification" simply classifies the entire image
  * "BinaryLocalization" uses a coarse grid of classifiers and does connected-components to return a list of SalientPoints, one for each connected components. This is the mode the current Person Detector uses. 
  * "AnchorBoxes" interprets Single Shot Detector (SSD) style outputs (bounding boxes)
  * "Segmentation" is a work in progress, but is designed to support networks which output classifications per pixel.
* `inputScale` and `inputShift` - When input to graph is _float_, data is first divided by scale and then shifted. I.e.:  `float_input = data / scale + shift`


### Model Storage

Model files are stored using git large file storage (LFS). Check them into `resources/config/engine/vision/dnn_models` as normal. There's a filter built into our repo that will automatically use LFS for `.pb` and `.tflite` files.

### Saving Images

For data collection, hard example mining, and debugging purposes, it is possible to save images that are processed by neural nets, along with Json files containing SalientPoints detected, if any. This currently relies on the DevImageCapture behavior, configured to use special class names in conjunction with one or more VisionModes enabled that use neural nets. These images are saved under `<cachePath>/vision/camera/images` and can be retrieved (or erased) using the `project/victor/scripts/get-dev-images.sh` (or `wipe-dev-images.sh`) scripts. 
  
  - If "FalsePositives" is the current class name, then the next image that results in a detection (a SalientPoint being created) will be saved after the backpack button is pressed.
  - If "FalseNegatives" is the current class name, then the next image that does _not_ result in a a detection (no SalientPoints returned) will be saved after the backpack button is pressed.


