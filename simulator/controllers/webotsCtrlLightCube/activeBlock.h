/*
 * File:          activeBlock.h
 * Date:
 * Description:   Main controller for simulated blocks and chargers
 * Author:        
 * Modifications: 
 */

#ifndef ACTIVE_BLOCK_H
#define ACTIVE_BLOCK_H

#include "coretech/common/shared/types.h"

namespace Anki {
  namespace Vector {
    namespace ActiveBlock {
  
      Result Init();
      void DeInit();
      
      Result Update();
  
    }  // namespace ActiveBlock
  }  // namespace Vector
}  // namespace Anki


#endif // ACTIVE_BLOCK_H
