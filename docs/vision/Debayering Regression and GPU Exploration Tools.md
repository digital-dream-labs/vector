# Debayering Regression and GPU Exploration Tools

Created by Patrick Doran Last updated Mar 25, 2019

In exploring debayering options on Vector, we needed several tools to test, explore, and compare options. Most notably, the tools were to explore Neon, GPU, CPU debayering and comparing their runtime and power consumption. While debayering will be done with a mix of Neon and CPU due efficiency related to power consumption, there may be future uses that these examples will provide help with; specifically ION memory and OpenCL with Qualcomm ION extension (Allows for doing zero copy). Alongside this are examples written to explore replacing Tensorflow Layers with custom GPU layers. The intention with the tensorflow example is to improve runtime of neural nets such that the trade off between increased power consumption vs runtime allows for a better response time to some detected higher-level input (e.g. person, hands, etc).


## Debayer Regression Tool
Located in platform/debayer, this tool exists so that we can know if we changed or broke something with debayering on Vector. To use it do the following:

```
# Build the tool
./project/victor/build-victor.sh -I -f -c Release -p vicos -D ANKI_BUILD_DEBAYER_REGRESSION_TOOL=1
 
# Copy to the robot
ssh robot
mount -o remount, rw /
exit
scp -r platform/debayer/resources _build/vicos/Release/bin/debayer <robot>:/home/root
 
# Run the regression test
ssh robot
./debayer resources/config.json
 
# If any of the values are different, an error message will appear
# Any created output will be in a directory called 'output'
```

This tool exists to both create uncompressed image files (just the rgb or grey bytes written to file) and to compare it to existing files. The files located in the platform/debayer/resources are the various possible outputs for vector written as the RGB or Y8 bytes. If there are any differences between the references files and the debayering, an error message will pop up


To view the images, you can use python and do the following:

```
python
 
from PIL import Image
import numpy as np
 
path = '<path>'
height = 720
width = 1280
channels = 3
 
Image.fromarray(np.fromfile(path, dtype=np.uint8).reshape((height, width, channels)).show()
```

## Exploratory Files
The exploration files are not production quality code. However, they do contain examples of how to use the frameworks.

* computation.tar.bz2
    * Multiple exploration files
* tflow-sandbox.bundle
    * Files for building and replacing tensorflow and tensorflow lite layers

Inside computation.tar.bz2 you will find:

* evaluation
    * Code used to explore and evaluate the different ways of using the GPU and Neon for debayering
    * There's a lot to unpack here, but these are the most relevant bits:
        * neon_faster.h
            * inline functions for doing approximate pow, exp, log, pow2 in neon for 4 channel floats
        * neon_debayer.h
            * Ignore this, it was experimental. Intended for drop in replacement into coretech
        * IONMemory.cpp, IONMemory.h
            * Wrapper around Qualcomm memory management
        * Evaluator.cpp, Evaluator.h
            * These files contain the setup and test code for the different approaches
            * The most useful functions
                * Evaluator::init
                    * Set up and initialization of OpenCL
                * Evaluator::zcp_allocate
                    * Function for setting up ION memory with the purpose of using it with OpenCL to do 'Zero Copy' 
                * Evaluator::zcp_copy
                    * Function for copying memory into a byte vector. Most just used as a precursor to writing bytes to file
                * Evaluator::gpu_debayer_zcp
                    * Function for doing Zero Copy debayering
                    * Uses ION Memory, Qualcomm OpenCL ION memory extension, and the OpenCL image2d type
                    * *This is the fastest we can do debayering on Vector's platform using all 10 bits of image data* (at the cost of additional power)
                * common_neon_debayer_u10
                    * Neon debayering using all 10 bits and computing gamma correction
                * common_neon_debayer_lut
                    * Neon debayering using the top 7 bits and a lookup table for gamma correction
                    * *This is the fastest and most power efficient we can do debayering on Vector's platform* (at the cost of losing the lower 3 bits of image data
        * global.cl
            * Contains functions used across multiple OpenCL files
            * The most useful functions
                * read_imagef_bayer_raw10
                    * Reads two blocks of RAW10 data, converts them to bytes, then converts the data into an array of eight floats.
                * write_imagef_bayer_rgb, write_imagef_bayer_grey
                    * Writes two blocks (i.e. eight floats from reading a raw10 image) into the rgb or grey outputs.
                * read_image_s_bayer_raw10
                    * Reads two blocks of RAW10 data, converts them to bytes, then converts the data into an array of eight shorts (u16)
                * write_image_s_bayer_rgb
                    * Writes two blocks (i.e. eight shorts from reading a raw10 image) into the rgb outputs using a lookup table to compute gamma
        * debayer.cl
            * Contains different debayering functions
            * The most useful functions
                * debayer_img
                    * Debayers a full sized image using the read/write functions found in global.cl
                * debayer_table
                    * Debayers a full sized image using the read/write functions found in global.cl that use a lookup table to compute gamma
        * squash.cl
            * Squashes the RAW10 bayer image into a RAW8 bayer image
        * resize.cl
            * Scales an arbitrary input image using the sampler specified in global.cl
* example
   * A program that uses OpenCL with C++ to do run a GPU kernel that applies ROT13 cipher to a string
   * This was used mostly to test how to link against OpenCL
* tfilte
   * Code used to explore replacing individual layers of tensorflow with a GPU based implementation.
   * This was intended as a hack to put GPU layers into neural nets for Vector.
   * Google announced on January 16, 2019 that they were working on TfLite GPU Delegate which they intend to release during 2019
        * It uses OpenGL ES 3.1 Compute Shaders.
        * Allegedly, Vector's hardware can support this
        * This announcement kind of makes any effort towards replacing layers unnecessary as we may have an easy way to integrate GPU support for TfLite within the year.

Inside tflow-sandbox.bundle you will find:

* tflow-sandbox
    * Complete example for creating a simple model, replacing a layer, and running the model with TfLite.
    * Also produces graphs of the model so you can see what's going on.