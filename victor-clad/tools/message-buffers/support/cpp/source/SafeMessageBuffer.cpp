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
  * File: SafeMessageBuffer.cpp
  *
  * Description: Utility for safe message serialization and deserialization.
  *
  **/

#include <CLAD/SafeMessageBuffer.h>
#include <cassert>
#include <cstring>

namespace CLAD
{

SafeMessageBuffer::SafeMessageBuffer()
  : _buffer(nullptr)
  , _bufferSize(0)
  , _writeHead(_buffer)
  , _readHead(_buffer)
  , _ownsBufferMemory(false)
{
}


SafeMessageBuffer::SafeMessageBuffer(size_t inSize)
  : _buffer(nullptr)
  , _bufferSize(0)
  , _writeHead(_buffer)
  , _readHead(_buffer)
  , _ownsBufferMemory(false)
{
  AllocateBuffer(inSize);
}


SafeMessageBuffer::SafeMessageBuffer(uint8_t* inBuffer, size_t inBufferSize, bool inOwnsBufferMemory)
  : _buffer(inBuffer)
  , _bufferSize(inBufferSize)
  , _writeHead(_buffer)
  , _readHead(_buffer)
  , _ownsBufferMemory(inOwnsBufferMemory)
{
}


SafeMessageBuffer::~SafeMessageBuffer()
{
  ReleaseBuffer();
}


void SafeMessageBuffer::ReleaseBuffer()
{
  if (_ownsBufferMemory)
    {
      delete[] _buffer;
      _ownsBufferMemory = false;
    }

  _buffer    = nullptr;
  _writeHead = _buffer;
  _readHead  = _buffer;
}


void SafeMessageBuffer::SetBuffer(uint8_t* inBuffer, size_t inBufferSize, bool inOwnsBufferMemory)
{
  ReleaseBuffer();

  _buffer           = inBuffer;
  _bufferSize       = inBufferSize;
  _writeHead        = _buffer;
  _readHead         = _buffer;
  _ownsBufferMemory = inOwnsBufferMemory;
}


void SafeMessageBuffer::AllocateBuffer(size_t inBufferSize)
{
  uint8_t* newBuffer = new uint8_t[inBufferSize];
  SetBuffer(newBuffer, inBufferSize, true);
}


size_t SafeMessageBuffer::GetBytesWritten() const
{
  return (_writeHead - _buffer);
}


size_t SafeMessageBuffer::GetBytesRead() const
{
  return (_readHead - _buffer);
}

size_t SafeMessageBuffer::CopyBytesOut(uint8_t* outBuffer, size_t bufferSize) const
{
  size_t byteCount = GetBytesWritten();
  if (bufferSize < byteCount) {
    return 0;
  }
  memcpy(outBuffer, _buffer, byteCount);
  return (byteCount);
}

void SafeMessageBuffer::Clear()
{
  memset(_buffer, 0, _bufferSize);
  _writeHead = _readHead = _buffer;
}

bool SafeMessageBuffer::WriteBytes(const void* srcData, size_t sizeOfWrite)
{
  const bool isRoomForWrite = (GetBytesWritten() + sizeOfWrite) <= _bufferSize;
  assert(isRoomForWrite);
  if (isRoomForWrite) {
    memcpy(_writeHead, srcData, sizeOfWrite);
    _writeHead += sizeOfWrite;
  }
  return isRoomForWrite;
}


bool SafeMessageBuffer::ReadBytes(void* destData, size_t sizeOfRead) const
{
  const bool isRoomForRead = ((GetBytesRead() + sizeOfRead) <= _bufferSize);
  assert(isRoomForRead);
  if (isRoomForRead) {
    memcpy(destData, _readHead, sizeOfRead);
    _readHead += sizeOfRead;
  }
  return isRoomForRead;
}

template<>
bool SafeMessageBuffer::Write<bool>(bool val)
{
  uint8_t tmp = val ? 1 : 0;
  return WriteBytes(&tmp, sizeof(tmp));
}

template<>
bool SafeMessageBuffer::Read<bool>(bool& outVal) const
{
  uint8_t tmp;
  bool readOk = Read(tmp);
  if (readOk) {
    outVal = (tmp != 0);
  }
  return readOk;
}

bool SafeMessageBuffer::ContentsEqual(const SafeMessageBuffer& other) const
{
    if (_buffer == nullptr || _bufferSize == 0) {
        return other._buffer == nullptr || other._bufferSize == 0;
    }
    else if (other._buffer == nullptr || other._bufferSize == 0) {
        return false;
    }

    size_t thisSize = (_writeHead > _readHead) ? _writeHead - _readHead : _bufferSize;
    uint8_t* thisBuffer = (_writeHead > _readHead) ? _readHead : _buffer;
    size_t otherSize = (other._writeHead > other._readHead) ? other._writeHead - other._readHead : other._bufferSize;
    uint8_t* otherBuffer = (other._writeHead > other._readHead) ? other._readHead : other._buffer;

    if (thisSize != otherSize) {
        return false;
    }
    else {
        return std::equal(thisBuffer, thisBuffer + thisSize, otherBuffer);
    }
}

} // CLAD
