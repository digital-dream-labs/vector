/**
 * File: visionModeSet.h
 *
 * Author: Andrew Stein
 * Date:   10/12/2018
 *
 * Description: Simple container for VisionModes.
 *
 * Copyright: Anki, Inc. 2018
 **/

#ifndef __Anki_Vector_Engine_VisionModeSet_H__
#define __Anki_Vector_Engine_VisionModeSet_H__

#include "clad/types/visionModes.h"

#include <set>

namespace Anki {
namespace Vector {

class VisionModeSet
{
public:
  
  VisionModeSet() = default;
  VisionModeSet(const std::initializer_list<VisionMode>& args);
  ~VisionModeSet() = default;

  // Insertion / Removal
  // TODO: switch to const& or && if/when VisionMode becomes a full-blown class (VIC-9525)
  
  // Single insertion
  void Insert(VisionMode mode) { _modes.insert(mode); };
  
  // Insert multiple VisionModes at once
  template<typename... VisionModes>
  void Insert(VisionModes&&... modes);
  
  template<class Container> // Statically asserts this is Container<VisionMode>
  void Insert(const Container& visionModes);
  
  // Insert all enumerated VisionModes
  void InsertAllModes();
  
  void Remove(VisionMode mode) { _modes.erase(mode);  };
  
  template<class Container> // Statically asserts this is Container<VisionMode>
  void Remove(const Container& modes);
  
  bool Contains(VisionMode mode) const { return (_modes.count(mode) > 0); };
  
  template<class Container>
  bool ContainsAnyOf(const Container& modes) const;
  
  bool IsEmpty() const { return _modes.empty(); }
  void Clear() { _modes.clear(); }
  
  // Enable=true amounts to Inserting a mode in the set.
  // Enable=false is the same as Removing from the set.
  // This is a convenience method to avoid if/else statements elsewhere
  void Enable(VisionMode mode, bool enable);
  
  template<class Container> // Statically asserts this is Container<VisionMode>
  void Enable(Container modes, bool enable);
  
  // Return the set intersection of this with other
  VisionModeSet Intersect(const VisionModeSet& other) const;

  // Returns string in the form "mode1+mode2+...+modeN", for all N modes in the set
  std::string ToString() const;

  const std::set<VisionMode>& GetSet() const { return _modes; }
  
  // Direct container access to use STL helpers
  using const_iterator = std::set<VisionMode>::const_iterator;
  size_t         size()  const { return _modes.size();  }
  const_iterator begin() const { return _modes.begin(); }
  const_iterator end()   const { return _modes.end();   }
  
private:

  std::set<VisionMode> _modes;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<typename... VisionModes>
void VisionModeSet::Insert(VisionModes&&... modes)
{
  _modes.insert({std::forward<VisionModes>(modes)...});
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<class Container>
void VisionModeSet::Insert(const Container& modes)
{
  static_assert(std::is_same<typename Container::value_type, VisionMode>::value,
                "Not a container of VisionModes");
  
  std::for_each(modes.begin(), modes.end(), [this](const VisionMode mode)
                {
                  Insert(mode);
                });
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<class Container>
bool VisionModeSet::ContainsAnyOf(const Container& modes) const
{
  static_assert(std::is_same<typename Container::value_type, VisionMode>::value,
                "Not a container of VisionModes");
  for(const auto& mode : modes)
  {
    if(Contains(mode))
    {
      return true;
    }
  }
  return false;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<class Container> // Statically asserts this is Container<VisionMode>
void VisionModeSet::Remove(const Container& modes)
{
  static_assert(std::is_same<typename Container::value_type, VisionMode>::value,
                "Not a container of VisionModes");
  
  std::for_each(modes.begin(), modes.end(), [this](const VisionMode mode)
                {
                  Remove(mode);
                });
}
  
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
template<class Container>
void VisionModeSet::Enable(Container modes, bool enable)
{
  static_assert(std::is_same<typename Container::value_type, VisionMode>::value,
                "Not a container of VisionModes");
  
  std::for_each(modes.begin(), modes.end(), [this, enable](const VisionMode mode)
                {
                  Enable(mode, enable);
                });
}
  
} // namespace Vector
} // namespace Anki

#endif /* __Anki_Vector_Engine_VisionModeSet_H__ */

