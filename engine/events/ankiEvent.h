/**
 * File: ankiEvent.h
 *
 * Author: Lee Crippen
 * Created: 07/30/15
 *
 * Description: Events that contain a simple type and a templatized data parameter.
 *
 * Copyright: Anki, Inc. 2015
 *
 * COZMO_PUBLIC_HEADER
 **/

#ifndef ANKI_COZMO_EVENT_H
#define ANKI_COZMO_EVENT_H

#include <stdint.h>
#include <utility>
#include <memory>


namespace Anki {
namespace Vector {

template <typename DataType>
class AnkiEvent
{
public:
  // In addition to the class being templated, this constructor is templated.
  // This allows for 'perfect forwarding' where the constructor either uses
  // a standard lvalue version or the c++11 rvalue reference version. Magic!
  template <typename FwdType>
  AnkiEvent(double time, uint32_t type, FwdType&& newData)
  : _currentTime(time)
  , _myType(type)
  , _data( std::make_shared<DataType>(std::forward<FwdType>(newData)) )
  { }

  template <typename FwdType>
  AnkiEvent(uint32_t type, FwdType&& newData)
  : _currentTime(0.0)
  , _myType(type)
  , _data( std::make_shared<DataType>(std::forward<FwdType>(newData)) )
  { }

  double GetCurrentTime() const { return _currentTime; }
  uint32_t GetType() const { return _myType; }
  const DataType& GetData() const { return *_data; }
  
protected:

  double _currentTime;
  uint32_t _myType;
  std::shared_ptr<DataType> _data;
  
}; // class Event


} // namespace Vector
} // namespace Anki

#endif //  ANKI_COZMO_EVENT_H
