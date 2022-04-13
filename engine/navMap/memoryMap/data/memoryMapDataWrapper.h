/**
 * File: memoryMapDataWrapper.h
 *
 * Author: Michael Willett
 * Date:   15/02/2018
 *
 * Description: Wrapper for shared pointer to prevent instantiating with null pointers, bad casting operations, 
 *   and enforce deep comparisons when using == and != operators.
 *
 * Copyright: Anki, Inc. 2018
 **/

#ifndef ANKI_COZMO_MEMORY_MAP_DATA_WRAPPER_H
#define ANKI_COZMO_MEMORY_MAP_DATA_WRAPPER_H

#include <type_traits>
#include <memory>
#include <assert.h>

namespace Anki {
namespace Vector {

class MemoryMapData;

template<class T, typename = std::enable_if<std::is_base_of<MemoryMapData, T>::value>>
class MemoryMapDataWrapper {
friend class MemoryMapData;

public:
  // make sure the copy constructor is only enabled if copying from an inherited to a base class
  template <typename S = T>
  MemoryMapDataWrapper(const MemoryMapDataWrapper<S>& other, 
    typename std::enable_if<std::is_base_of<T, S>::value>::type* = nullptr)
  : dataPtr(other.GetSharedPtr()) {}

  // trivial type constructors
  MemoryMapDataWrapper() : dataPtr(std::make_shared<T>()) {} 
  MemoryMapDataWrapper(const T& other) : dataPtr(std::make_shared<T>(other)) {} 

  bool operator==(const MemoryMapDataWrapper<T>& other) const { return   dataPtr->Equals(other.dataPtr.get());  }
  bool operator!=(const MemoryMapDataWrapper<T>& other) const { return !(dataPtr->Equals(other.dataPtr.get())); }

  MemoryMapDataWrapper<T>&  operator=(const MemoryMapDataWrapper& other)     { dataPtr = other.GetSharedPtr(); return *this; }
  T*                        operator->()                               const { return dataPtr.get(); }
  std::shared_ptr<T>        GetSharedPtr()                             const { return dataPtr; }

private:
  MemoryMapDataWrapper(std::shared_ptr<T> ptr) : dataPtr(ptr) { assert(ptr); }
  std::shared_ptr<T> dataPtr;
};

} // namespace Vector
} // namespace Anki

#endif