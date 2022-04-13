/**
 * File: encodedImage.h
 *
 * Author: Andrew Stein
 * Date:   6/9/2016
 *
 * Description: Defines a container for encoded images on the basestation.
 *
 * Copyright: Anki, Inc. 2016
 **/

#ifndef __Anki_Vision_Basestation_EncodedImage_H__
#define __Anki_Vision_Basestation_EncodedImage_H__

#include "coretech/common/engine/robotTimeStamp.h"
#include "clad/types/imageTypes.h"

#include <vector>


namespace Anki {
  
  // Forward declaration
  namespace Vision {
    class Image;
    class ImageRGB;
  }
  
namespace Vector {
  
  class EncodedImage
  {
  public:
    
    EncodedImage();
    // Create an "encoded" image from an existing image. Copies data from the image
    // into the EncodedImage's buffer. ImageEncoding will be RawGray or RawRGB.
    explicit EncodedImage(const Vision::Image& imgGray, const u32 imageID = std::numeric_limits<u32>::max());
    explicit EncodedImage(const Vision::ImageRGB& imgRGB, const u32 imageID = std::numeric_limits<u32>::max());
    
    // Returns true if the image is ready after adding this chunk
    bool AddChunk(const ImageChunk& chunk);
    
    void Clear() { _buffer.clear(); }
    bool IsEmpty() const { return _buffer.empty(); }
    
    bool IsColor() const;
    
    u32 GetImageID() const { return _imgID; }
    s32 GetWidth()   const { return _imgWidth; }
    s32 GetHeight()  const { return _imgHeight; }
    
    RobotTimeStamp_t GetTimeStamp() const { return _timestamp; }
    
    // Decode internal buffer into a given gray or RGB image
    Result DecodeImageRGB(Vision::ImageRGB& decodedImg) const;
    Result DecodeImageGray(Vision::Image& decodedImg) const;
    
    Result Save(const std::string& filename) const;
    
    const RobotTimeStamp_t GetPrevTimestamp() const { return _prevTimestamp; }
    void SetPrevTimestamp(const RobotTimeStamp_t timestamp) { _prevTimestamp = timestamp; }
    
  private:

    std::vector<u8>        _buffer;
    
    RobotTimeStamp_t       _timestamp;
    RobotTimeStamp_t       _prevTimestamp;
    s32                    _imgWidth;
    s32                    _imgHeight;
    u32                    _imgID;
    
    Vision::ImageEncoding  _encoding;
    u8                     _expectedChunkId;
    bool                   _isImgValid;
    u8                     _numChunksReceived;
    

    static void MiniGrayToJpeg(const std::vector<u8>& bufferIn,
                               const u16 height,
                               const u16 width,
                               std::vector<u8>& bufferOut);
    
    static void MiniColorToJpeg(const std::vector<u8>& bufferIn,
                                const u16 height,
                                const u16 width,
                                std::vector<u8>& bufferOut);
    
    static void MiniToJpegHelper(const std::vector<u8>& bufferIn,
                                 const u16 height,
                                 const u16 width,
                                 std::vector<u8>& bufferOut,
                                 const u8* header,
                                 const size_t headerSize);
    
    template<class ImageType>
    Result DecodeImageHelper(ImageType& decodedImg) const;
    
  }; // class EncodedImage

} // namespace Vector
} // namespace Anki

#endif // __Anki_Vision_Basestation_EncodedImage_H__
