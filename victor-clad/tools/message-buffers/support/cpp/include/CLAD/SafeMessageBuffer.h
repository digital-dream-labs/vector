/*
 * Copyright 2015-2016 Anki Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * File: SafeMessageBuffer.h
 *
 * Description: Utility for safe message serialization and deserialization.
 *
 **/

#ifndef CLAD_SAFEMESSAGEBUFFER_H_
#define CLAD_SAFEMESSAGEBUFFER_H_

#ifndef __cplusplus
#error "SafeMessageBuffer is a C++ only header!"
#endif

#include <cassert>
#include <cstdint>
#include <vector>
#include <string>
#include <array>

namespace CLAD
{

class __attribute__((visibility("default"))) SafeMessageBuffer
{
public:

  // ========== Constructors / Destructors ==========

  SafeMessageBuffer();
  explicit SafeMessageBuffer(size_t inSize);
  SafeMessageBuffer(uint8_t* inBuffer, size_t inBufferSize, bool inOwnsBufferMemory = false);

  ~SafeMessageBuffer();

  // ========== Buffer Management ==========

  void AllocateBuffer(size_t inBufferSize);
  void SetBuffer(uint8_t* inBuffer, size_t inBufferSize, bool inOwnsBufferMemory = false);
  void ReleaseBuffer();

  size_t  GetBytesWritten() const;
  size_t  GetBytesRead() const;
  size_t  CopyBytesOut(uint8_t* outBuffer, size_t bufferSize) const;

  void Clear();

  // ========== Write methods ==========

  bool WriteBytes(const void* srcData, size_t sizeOfWrite);

  template <typename T>
  bool Write(T inVal)
  {
    return WriteBytes(&inVal, sizeof(inVal));
  }

  template <typename T>
  bool Write( const std::vector<T>& inVec)
  {
    for(const T& val : inVec) {
      if(!Write(val)) {
        return false;
      }
    }
    return true;
  }

  template <typename val_t, size_t length>
  bool WriteFArray(const std::array<val_t, length>& inArray)
  {
    for(const val_t& val : inArray) {
      if(!Write(val)) {
        return false;
      }
    }
    return true;
  }

  template <typename val_t, typename length_t>
  bool WriteVArray(val_t* arrayPtr, const length_t numElements)
  {
    bool wroteOK = Write(numElements);
    if (wroteOK && (numElements != 0))
    {
      wroteOK |= WriteBytes(arrayPtr, sizeof(val_t) * numElements);
    }
    return wroteOK;
  }

  template <typename val_t, typename length_t>
  bool WriteVArray( const std::vector<val_t>& inVec )
  {
    const auto inVecLength = inVec.size();
    const length_t lengthWritten = static_cast<length_t>(inVecLength);
    assert(static_cast<decltype(inVecLength)>(lengthWritten) == inVecLength);

    if(!Write(lengthWritten)) {
      return false;
    }
    if(!Write(inVec)) {
      return false;
    }
    return true;
  }

  template <typename length_t>
  bool WritePString( const std::string& str )
  {
    const std::size_t strlen = str.length();
    const length_t lengthWritten = static_cast<length_t>(strlen);
    assert(static_cast<std::size_t>(lengthWritten) == strlen);

    if(!Write(lengthWritten)) {
      return false;
    }
    if(lengthWritten > 0) {
      if(!WriteBytes(str.data(), lengthWritten)) {
        return false;
      }
    }
    return true;
  }

  template <typename array_length_t, typename string_length_t>
  bool WritePStringVArray( const std::vector<std::string>& inVec )
  {
    const auto inVecLength = inVec.size();
    const array_length_t lengthWritten = static_cast<array_length_t>(inVecLength);
    assert(static_cast<decltype(inVecLength)>(lengthWritten) == inVecLength);

    if(!Write(lengthWritten)) {
      return false;
    }
    // In event of truncation of length, write that number of items
    const size_t numToWrite = lengthWritten;
    for (size_t i = 0; i < numToWrite; ++i) {
      const std::string& str = inVec[i];
      if(!WritePString<string_length_t>(str)) {
        return false;
      }
    }
    return true;
  }

  template <size_t length, typename string_length_t>
  bool WritePStringFArray(const std::array<std::string, length>& inArray)
  {
    for(const std::string& val : inArray) {
      if(!WritePString<string_length_t>(val)) {
        return false;
      }
    }
    return true;
  }

  // ========== Read methods ==========

  bool ReadBytes(void* destData, size_t sizeOfRead) const;

  template <typename T>
  bool Read(T& outVal) const
  {
    return ReadBytes(&outVal, sizeof(outVal));
  }

  template <typename val_t>
  bool Read( std::vector<val_t>& outVec, const size_t num) const
  {
    outVec.clear();
    outVec.reserve(num);
    val_t val;
    for(size_t i = 0; i < num; i++) {
      if(!Read(val)) {
        return false;
      }
      outVec.push_back(val);
    }
    return outVec.size() == num;
  }

  template <typename val_t, size_t length>
  bool ReadFArray( std::array<val_t, length>& outArray ) const
  {
    val_t val;
    for(size_t i = 0; i < length; i++) {
      if(!Read(val)) {
        return false;
      }
      outArray[i] = val;
    }
    return true;
  }

  template <typename val_t, typename length_t>
  bool ReadVArray( std::vector<val_t>& outVec ) const
  {
    length_t length;
    if(!Read(length)) {
      return false;
    }
    if(!Read(outVec, length)) {
      return false;
    }
    return true;
  }

  template <typename length_t>
  bool ReadPString( std::string& outStr ) const
  {
    length_t length;
    if(!Read(length)) {
      return false;
    }
    if(!ReadString(outStr, length)) {
      return false;
    }
    return true;
  }
  
  bool ReadString( std::string& outStr, size_t length) const
  {
    outStr.clear();
    if(length > 0) {
      outStr.reserve(length);
      // This is probably really slow!
      // TODO: figure out how to more quickly unpack a string
      std::string::value_type val;
      for(size_t i = 0; i < length; i++) {
        if(!Read(val)) {
          return false;
        }
        outStr.push_back(val);
      }
    }
    return outStr.length() == length;
  }

  template <typename array_length_t, typename string_length_t>
  bool ReadPStringVArray( std::vector<std::string>& outVec ) const
  {
    array_length_t length;
    if(!Read(length)) {
      return false;
    }
    const size_t num = length;
    outVec.clear();
    outVec.reserve(num);
    std::string val;
    for(size_t i = 0; i < num; i++) {
      if(!ReadPString<string_length_t>(val)) {
        return false;
      }
      outVec.push_back(val);
    }
    return outVec.size() == num;
  }

  template <size_t length, typename string_length_t>
  bool ReadPStringFArray( std::array<std::string, length>& outArray ) const
  {
    std::string val;
    for(size_t i = 0; i < length; i++) {
      if(!ReadPString<string_length_t>(val)) {
        return false;
      }
      outArray[i] = val;
    }
    return true;
  }

  template <typename val_t>
  bool ReadCompoundTypeVec( std::vector<val_t>& outVec, const size_t num) const
  {
    outVec.clear();
    outVec.reserve(num);
    val_t val;
    for(size_t i = 0; i < num; i++) {
      if(!val.Unpack(*this)) {
        return false;
      }
      outVec.push_back(val);
    }
    return outVec.size() == num;
  }

  template <typename val_t, typename length_t>
  bool ReadCompoundTypeVArray( std::vector<val_t>& outVec ) const
  {
    length_t length;
    if(!Read(length)) {
      return false;
    }
    if(!ReadCompoundTypeVec(outVec, length)) {
      return false;
    }
    return true;
  }

  template <typename val_t, size_t length>
  bool ReadCompoundTypeFArray( std::array<val_t, length>& outArray ) const
  {
    val_t val;
    for(size_t i = 0; i < length; i++) {
      if(!val.Unpack(*this)) {
        return false;
      }
      outArray[i] = val;
    }
    return true;
  }

  // Buffer contents equality
  bool ContentsEqual(const SafeMessageBuffer& other) const;

private:

  SafeMessageBuffer(const SafeMessageBuffer&);
  SafeMessageBuffer& operator=(const SafeMessageBuffer&);

  // ========== Member Data ==========

  uint8_t*          _buffer;
  size_t            _bufferSize;

  uint8_t*          _writeHead;
  mutable uint8_t*  _readHead;

  bool              _ownsBufferMemory;
};


template<>
bool SafeMessageBuffer::Write(bool inVal);

template<>
bool SafeMessageBuffer::Read(bool& outVal) const;


}// CLAD

#endif // SAFEMESSAGEBUFFER_H_
