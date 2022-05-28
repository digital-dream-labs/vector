#ifndef __SIMPLE_SIGNAL_FWD_H__
#define __SIMPLE_SIGNAL_FWD_H__

#include <cstddef>
#include <memory>

namespace Signal {
  
  namespace Lib {
    class ScopedHandleContainer;
  }
  using SmartHandle = std::shared_ptr<Lib::ScopedHandleContainer>;
  
}

#endif // __SIMPLE_SIGNAL_FWD_H__
