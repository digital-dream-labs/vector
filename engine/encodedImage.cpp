/**
 * File: encodedImage.cpp
 *
 * Author: Andrew Stein
 * Date:   6/9/2016
 *
 * Description: Implements a container for encoded images on the basestation.
 *
 * Copyright: Anki, Inc. 2016
 **/

#include "engine/encodedImage.h"
#include "coretech/vision/engine/image.h"
#include "coretech/common/shared/array2d.h"
#include "anki/cozmo/shared/cozmoConfig.h"

#include "util/fileUtils/fileUtils.h"

#if ANKICORETECH_USE_OPENCV
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#endif

namespace Anki {
namespace Vector {
  
  EncodedImage::EncodedImage()
  : _timestamp(0)
  , _prevTimestamp(0)
  , _imgWidth(0)
  , _imgHeight(0)
  , _imgID(std::numeric_limits<u32>::max())
  , _encoding(Vision::ImageEncoding::NoneImageEncoding)
  , _expectedChunkId(0)
  , _isImgValid(false)
  , _numChunksReceived(0)
  {
    
  }
  
  EncodedImage::EncodedImage(const Vision::Image& imgGray, const u32 imageID)
  : _timestamp(imgGray.GetTimestamp())
  , _imgWidth(imgGray.GetNumCols())
  , _imgHeight(imgGray.GetNumRows())
  , _imgID(imageID)
  , _encoding(Vision::ImageEncoding::RawGray)
  , _isImgValid(!imgGray.IsEmpty())
  {
    _buffer.reserve(imgGray.GetNumElements());
    std::copy(imgGray.GetDataPointer(), imgGray.GetDataPointer() + imgGray.GetNumElements(),
              std::back_inserter(_buffer));
  }
  
  EncodedImage::EncodedImage(const Vision::ImageRGB& imgRGB, const u32 imageID)
  : _timestamp(imgRGB.GetTimestamp())
  , _imgWidth(imgRGB.GetNumCols())
  , _imgHeight(imgRGB.GetNumRows())
  , _imgID(imageID)
  , _encoding(Vision::ImageEncoding::RawRGB)
  , _isImgValid(!imgRGB.IsEmpty())
  {
    _buffer.reserve(imgRGB.GetNumElements() * 3);
    
    s32 nrows = imgRGB.GetNumRows(), ncols = imgRGB.GetNumCols();
    if(imgRGB.IsContinuous())
    {
      ncols *= nrows;
      nrows = 1;
    }
    
    for(s32 i=0; i<nrows; ++i)
    {
      const Vision::PixelRGB* img_i = imgRGB.GetRow(i);
      for(s32 j=0; j<ncols; ++j)
      {
        _buffer.push_back(img_i[j].r());
        _buffer.push_back(img_i[j].g());
        _buffer.push_back(img_i[j].b());
      }
    }
  }

  bool EncodedImage::AddChunk(const ImageChunk &chunk)
  {
    if(chunk.data.size() > static_cast<u32>(ImageConstants::IMAGE_CHUNK_SIZE)) {
      PRINT_NAMED_WARNING("EncodedImage.AddChunk.ChunkTooBig",
                          "Expecting chunks of size no more than %d, got %zu.",
                          ImageConstants::IMAGE_CHUNK_SIZE, chunk.data.size());
      return false;
    }
    
    // If image ID has changed, then start over.
    if (chunk.imageId != _imgID)
    {
      _imgID = chunk.imageId;

      _imgWidth        = chunk.width;
      _imgHeight       = chunk.height;
      _isImgValid      = (chunk.chunkId == 0);
      _expectedChunkId = 0;
      _encoding        = chunk.imageEncoding;
      
      // If the encoding is MiniGray then first byte of the image is whether or not the image is in color
      // (The encoding is hard coded deep within firmware code and can't easily be changed so this is a work
      // around)
      // If it is in color then update the encoding
      if(chunk.data[0] != 0 && _encoding == Vision::ImageEncoding::JPEGMinimizedGray)
      {
        _encoding = Vision::ImageEncoding::JPEGMinimizedColor;
      }

      _buffer.clear();
      
      if(chunk.imageEncoding == Vision::ImageEncoding::JPEGGray)
      {
        _buffer.reserve(_imgWidth*_imgHeight);
      }
      else
      {
        _buffer.reserve(_imgWidth*_imgHeight*sizeof(Vision::PixelRGB));
      }
      
      _numChunksReceived = 0;
    }
    
    // Check if a chunk was received out of order
    if (chunk.chunkId != _expectedChunkId) {
      PRINT_NAMED_WARNING("EncodedImage.AddChunk.ChunkOutOfOrder",
                          "Expected chunk %d, got chunk %d", _expectedChunkId, chunk.chunkId);
      _isImgValid = false;
    }
    
    _expectedChunkId = chunk.chunkId + 1;
    ++_numChunksReceived;
    
    // We've received all data when the msg chunkSize is less than the max
    const bool isLastChunk =  chunk.chunkId == chunk.imageChunkCount-1;
    if(isLastChunk) {
      // Check if we received as many chunks as we should have
      if(_numChunksReceived != chunk.imageChunkCount)
      {
        PRINT_NAMED_WARNING("EncodedImage.AddChunk.UnexpectedNumberOfChunks",
                            "Got last chunk, expected %d chunks but received %d chunks",
                            chunk.imageChunkCount,
                            _numChunksReceived);
        _isImgValid = false;
      }
      else
      {
        // Set timestamp using last chunk
        _prevTimestamp = _timestamp;
        _timestamp = chunk.frameTimeStamp;
        
        if(_prevTimestamp > _timestamp)
        {
          PRINT_NAMED_WARNING("EncodedImage.AddChunk.TimestampNotIncreasing",
                              "Got last chunk but current timestamp %u is less than previous timestamp %u",
                              (TimeStamp_t)_timestamp,
                              (TimeStamp_t)_prevTimestamp);
          _isImgValid = false;
        }
      }
    }
    
    if (!_isImgValid) {
      if (isLastChunk) {
        PRINT_NAMED_INFO("EncodedImage.AddChunk.IncompleteImage",
                         "Received last chunk of invalidated image");
      }
      return false;
    }
    
    // Image chunks are assumed/guaranteed to be received in order so  we just
    // blindly append data to array
    _buffer.insert(_buffer.end(), chunk.data.begin(), chunk.data.end());
    
    return isLastChunk;
  }
  
  bool EncodedImage::IsColor() const
  {
    switch(_encoding)
    {
      case Vision::ImageEncoding::NoneImageEncoding:
      {
        ANKI_VERIFY(false, "EncodedImage.IsColor.UnsupportedImageEncoding", "%s", EnumToString(_encoding));
        // Intentional fallthrough!
      }
        
      case Vision::ImageEncoding::JPEGGray:
      case Vision::ImageEncoding::JPEGMinimizedGray:
      case Vision::ImageEncoding::RawGray:
      {
        return false;
      }
        
      case Vision::ImageEncoding::JPEGColor:
      case Vision::ImageEncoding::JPEGMinimizedColor:
      case Vision::ImageEncoding::JPEGColorHalfWidth:
      case Vision::ImageEncoding::RawRGB:
      case Vision::ImageEncoding::YUYV:
      case Vision::ImageEncoding::YUV420sp:
      case Vision::ImageEncoding::BAYER:
      {
        return true;
      }
    }
  }
  
  static inline void DecodeHelper(const std::vector<u8>& buffer, Vision::ImageRGB& decodedImg)
  {
    cv::imdecode(buffer, cv::IMREAD_COLOR, &decodedImg.get_CvMat_());
    cvtColor(decodedImg.get_CvMat_(), decodedImg.get_CvMat_(), CV_BGR2RGB); // opencv will decode as BGR
  }
  
  static inline void DecodeHelper(const std::vector<u8>& buffer, Vision::Image& decodedImg)
  {
    cv::imdecode(buffer, cv::IMREAD_GRAYSCALE, &decodedImg.get_CvMat_());
  }


  // General template for when Gray or RGB data is both passed in and also requested out.
  template<class ImgTypeIn, class ImgTypeOut>
  static inline void RawHelper(const ImgTypeIn& imgIn, ImgTypeOut& imgOut)
  {
    // TODO: Avoid copying the data
    //   (for now, it's necessary because we can't guarantee _buffer will be
    //    valid, yet imgIn is just a header around its data
    imgIn.CopyTo(imgOut);
  }
  
  // Specialization for converting gray data to rgb data
  template<>
  inline void RawHelper(const Vision::Image& grayImgIn, Vision::ImageRGB& rgbImgOut)
  {
    rgbImgOut = Vision::ImageRGB(grayImgIn);
  }
  
  // Specialization for converting rgb data to gray data
  template<>
  inline void RawHelper(const Vision::ImageRGB& rgbImageIn, Vision::Image& grayImgOut)
  {
    grayImgOut = rgbImageIn.ToGray();
  }
  
  
  template<class ImageType>
  Result EncodedImage::DecodeImageHelper(ImageType& decodedImg) const
  {
    switch(_encoding)
    {
      case Vision::ImageEncoding::JPEGColor:
      case Vision::ImageEncoding::JPEGGray:
      {
        // Simple case: just decode directly into the passed-in image's buffer.
        // Note that this will take a buffer with grayscale data in it and
        // turn it into RGB for us.
        DecodeHelper(_buffer, decodedImg);
        break;
      }
        
      case Vision::ImageEncoding::JPEGMinimizedGray:
      {
        // Convert our special minimized JPEG format to regular JPEG buffer and
        // decode that
        std::vector<u8> tempBuffer;
        MiniGrayToJpeg(_buffer, _imgHeight, _imgWidth, tempBuffer);
        DecodeHelper(tempBuffer, decodedImg);
        break;
      }
      
      case Vision::ImageEncoding::RawGray:
      {
        // Already decompressed.
        Vision::Image grayImg(_imgHeight, _imgWidth, const_cast<u8*>(&(_buffer[0])));
        RawHelper(grayImg, decodedImg);
        break;
      }
        
      case Vision::ImageEncoding::RawRGB:
      {
        // Already decompressed.
        Vision::ImageRGB rgbImg(_imgHeight, _imgWidth, const_cast<u8*>(&(_buffer[0])));
        RawHelper(rgbImg, decodedImg);
        break;
      }
        
      case Vision::ImageEncoding::JPEGColorHalfWidth:
      {
        DecodeHelper(_buffer, decodedImg);
        cv::copyMakeBorder(decodedImg.get_CvMat_(), decodedImg.get_CvMat_(),
                           0, 0, 160, 160, cv::BORDER_CONSTANT, 0);
        break;
      }
      
      case Vision::ImageEncoding::JPEGMinimizedColor:
      {
        // Convert our special minimized JPEG format to regular JPEG buffer and
        // decode that
        std::vector<u8> tempBuffer;
        
        // Color images are half width
        MiniColorToJpeg(_buffer, _imgHeight, _imgWidth/2, tempBuffer);
        DecodeHelper(tempBuffer, decodedImg);
        decodedImg.Resize(_imgHeight, _imgWidth);
        break;
      }
        
      default:
        PRINT_NAMED_ERROR("EncodedImage.DecodeImageRGB.UnsupportedEncoding",
                          "Encoding %s not yet supported for decoding image chunks",
                          EnumToString(_encoding));
        
        return RESULT_FAIL;
        
    } // switch(encoding)
    
    if(decodedImg.GetNumRows() != _imgHeight || decodedImg.GetNumCols() != _imgWidth)
    {
      PRINT_NAMED_WARNING("EncodedImage.DecodeImageRGB.BadDecode",
                          "Failed to decode %dx%d image from buffer. Got %dx%d",
                          _imgWidth, _imgHeight, decodedImg.GetNumCols(), decodedImg.GetNumRows());
      
      return RESULT_FAIL;
    }
    
    decodedImg.SetTimestamp((TimeStamp_t)_timestamp);
    
    return RESULT_OK;
  }

  
  Result EncodedImage::DecodeImageRGB(Vision::ImageRGB& decodedImg) const
  {
    return DecodeImageHelper(decodedImg);
  }

  Result EncodedImage::DecodeImageGray(Vision::Image& decodedImg) const
  {
    return DecodeImageHelper(decodedImg);
  }
  
  Result EncodedImage::Save(const std::string& filename) const
  {
    const std::vector<u8>* bufferPtr = &_buffer;
    std::vector<u8> tempBuffer;
    
    if(_encoding == Vision::ImageEncoding::JPEGMinimizedGray)
    {
      // If this buffer is encoded as our homebrew "MinimizedGray" JPEG,
      // we need to convert it for storage so it can be read by normal
      // JPEG decoders
      MiniGrayToJpeg(_buffer, _imgHeight, _imgWidth, tempBuffer);
      bufferPtr = &tempBuffer;
    }
    else if(_encoding == Vision::ImageEncoding::JPEGMinimizedColor)
    {
      // Special case: our homebrew "MinimizedColor" images are half width,
      // so have to fully decode, which resizes to full size, and then save
      // (which also re-compresses as a normal JPEG so it can be read by
      // normal decoders)
      Vision::ImageRGB decodedImg;
      Result result = DecodeImageRGB(decodedImg);
      if(RESULT_OK != result) {
        PRINT_NAMED_WARNING("EncodedImage.Save.DecodeColorFailed", "");
        return result;
      }
      
      result = decodedImg.Save(filename);
      if(RESULT_OK != result) {
        PRINT_NAMED_WARNING("EncodedImage.Save.MiniJPEGSaveFailed", "");
      }
      
      return result;
    }
    
    const bool success = Util::FileUtils::WriteFile(filename, *bufferPtr);
    
    if(success) {
      return RESULT_OK;
    } else {
      PRINT_NAMED_WARNING("EncodedImage.Save.WriteFail", "Filename: %s",
                          filename.c_str());
      return RESULT_FAIL;
    }
  }
  
  // Turn a fully assembled MINIPEG_GRAY image into a JPEG with header and footer
  // This is a port of C# code from Nathan.
  void EncodedImage::MiniGrayToJpeg(const std::vector<u8>& bufferIn, const u16 height, const u16 width,
                                    std::vector<u8>& bufferOut)
  {
    // Fetch quality to decide which header to use
    //const int quality = bufferIn[0];
    const int quality = 50;
    
    // Pre-baked JPEG header for grayscale, Q50
    static const u8 header50[] = {
      0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
      0x00, 0x01, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x10, 0x0B, 0x0C, 0x0E, 0x0C, 0x0A, 0x10, // 0x19 = QTable
      0x0E, 0x0D, 0x0E, 0x12, 0x11, 0x10, 0x13, 0x18, 0x28, 0x1A, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
      0x25, 0x1D, 0x28, 0x3A, 0x33, 0x3D, 0x3C, 0x39, 0x33, 0x38, 0x37, 0x40, 0x48, 0x5C, 0x4E, 0x40,
      0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6D, 0x51, 0x57, 0x5F, 0x62, 0x67, 0x68, 0x67, 0x3E, 0x4D,
      
      //0x71, 0x79, 0x70, 0x64, 0x78, 0x5C, 0x65, 0x67, 0x63, 0xFF, 0xC0, 0x00, 0x0B, 0x08, 0x00, 0xF0, // 0x5E = Height x Width
      0x71, 0x79, 0x70, 0x64, 0x78, 0x5C, 0x65, 0x67, 0x63, 0xFF, 0xC0, 0x00, 0x0B, 0x08, 0x01, 0x28, // 0x5E = Height x Width
      
      //0x01, 0x40, 0x01, 0x01, 0x11, 0x00, 0xFF, 0xC4, 0x00, 0xD2, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01,
      0x01, 0x90, 0x01, 0x01, 0x11, 0x00, 0xFF, 0xC4, 0x00, 0xD2, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01,
      
      0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04,
      0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
      0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
      0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
      0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16,
      0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
      0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
      0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
      0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
      0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
      0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
      0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
      0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01,
      0x00, 0x00, 0x3F, 0x00
    };
    
    // Pre-baked JPEG header for grayscale, Q80
    static const u8 header80[] = {
      0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
      0x00, 0x01, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x06, 0x04, 0x05, 0x06, 0x05, 0x04, 0x06,
      0x06, 0x05, 0x06, 0x07, 0x07, 0x06, 0x08, 0x0A, 0x10, 0x0A, 0x0A, 0x09, 0x09, 0x0A, 0x14, 0x0E,
      0x0F, 0x0C, 0x10, 0x17, 0x14, 0x18, 0x18, 0x17, 0x14, 0x16, 0x16, 0x1A, 0x1D, 0x25, 0x1F, 0x1A,
      0x1B, 0x23, 0x1C, 0x16, 0x16, 0x20, 0x2C, 0x20, 0x23, 0x26, 0x27, 0x29, 0x2A, 0x29, 0x19, 0x1F,
      0x2D, 0x30, 0x2D, 0x28, 0x30, 0x25, 0x28, 0x29, 0x28, 0xFF, 0xC0, 0x00, 0x0B, 0x08, 0x00, 0xF0,
      0x01, 0x40, 0x01, 0x01, 0x11, 0x00, 0xFF, 0xC4, 0x00, 0xD2, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04,
      0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
      0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
      0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
      0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16,
      0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
      0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
      0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
      0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
      0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
      0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
      0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
      0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01,
      0x00, 0x00, 0x3F, 0x00
    };
    
    const u8* header = nullptr;
    size_t headerSize = 0;
    switch(quality)
    {
      case 50:
        header = header50;
        headerSize = sizeof(header50);
        break;
      case 80:
        header = header80;
        headerSize = sizeof(header80);
        break;
      default:
        PRINT_NAMED_ERROR("miniGrayToJpeg", "No header for quality of %d", quality);
        return;
    }
    
    MiniToJpegHelper(bufferIn, height, width, bufferOut, header, headerSize);
  }
  
  // Turn a fully assembled MINIPEG_COLOR image into a JPEG with header and footer
  // This is a port of python code from Nathan.
  void EncodedImage::MiniColorToJpeg(const std::vector<u8>& bufferIn, const u16 height, const u16 width,
                                     std::vector<u8>& bufferOut)
  {
    // Pre-baked JPEG header for color, Q50
    static const u8 header[] = {
      0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01,
      0x00, 0x01, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x10, 0x0B, 0x0C, 0x0E, 0x0C, 0x0A, 0x10, // 0x19 = QTable
      0x0E, 0x0D, 0x0E, 0x12, 0x11, 0x10, 0x13, 0x18, 0x28, 0x1A, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23,
      0x25, 0x1D, 0x28, 0x3A, 0x33, 0x3D, 0x3C, 0x39, 0x33, 0x38, 0x37, 0x40, 0x48, 0x5C, 0x4E, 0x40,
      0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6D, 0x51, 0x57, 0x5F, 0x62, 0x67, 0x68, 0x67, 0x3E, 0x4D,
      0x71, 0x79, 0x70, 0x64, 0x78, 0x5C, 0x65, 0x67, 0x63, 0xFF, 0xC0, 0x00, 17, // 8+3*components
      0x08, 0x00, 0xF0, // 0x5E = Height x Width
      0x01, 0x40,
      0x03, // 3 components
      0x01, 0x21, 0x00, // Y 2x1 res
      0x02, 0x11, 0x00, // Cb
      0x03, 0x11, 0x00, // Cr
      0xFF, 0xC4, 0x00, 0xD2, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04,
      0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03,
      0x05, 0x05, 0x04, 0x04, 0x00, 0x00, 0x01, 0x7D, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
      0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08,
      0x23, 0x42, 0xB1, 0xC1, 0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16,
      0x17, 0x18, 0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
      0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
      0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
      0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
      0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6,
      0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4,
      0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,
      0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA,
      0xFF, 0xDA, 0x00, 12,
      0x03, // 3 components
      0x01, 0x00, // Y
      0x02, 0x00, // Cb same AC/DC
      0x03, 0x00, // Cr same AC/DC
      0x00, 0x3F, 0x00
    };
    
    MiniToJpegHelper(bufferIn, height, width, bufferOut, header, sizeof(header));
  }
    
  void EncodedImage::MiniToJpegHelper(const std::vector<u8>& bufferIn,
                                      const u16 height,
                                      const u16 width,
                                      std::vector<u8>& bufferOut,
                                      const u8* header,
                                      const size_t headerLength)
  {
    assert(header != nullptr);
    assert(headerLength > 0);
    
    // Allocate enough space for worst case expansion
    size_t bufferLength = bufferIn.size();
    bufferOut.reserve(bufferLength*2 + headerLength);
    
    bufferOut.insert(bufferOut.begin(), header, header+headerLength);
    
    // Adjust header size information
    assert(bufferOut.size() > 0x61);
    bufferOut[0x5e] = height >> 8;
    bufferOut[0x5f] = height & 0xff;
    bufferOut[0x60] = width  >> 8;
    bufferOut[0x61] = width  & 0xff;
    
    while (bufferIn[bufferLength-1] == 0xff) {
      bufferLength--; // Remove trailing 0xFF padding
    }
    
    // Add byte stuffing - one 0 after each 0xff
    for (int i = 1; i < bufferLength; i++)
    {
      bufferOut.push_back(bufferIn[i]);
      if (bufferIn[i] == 0xff) {
        bufferOut.push_back(0);
      }
    }
    
    bufferOut.push_back(0xFF);
    bufferOut.push_back(0xD9);
  }
  
} // namespace Vector
} // namespace Anki
