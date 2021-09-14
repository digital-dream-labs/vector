/**
 * File: audioKeyFrameTypes.cpp
 *
 * Authors: Jordan Rivas
 * Created: 02/28/18
 *
 * Description:
 *   Structs to define Audio key frame types. These structs are used to load Animation data into Audio Key Frames.
 *
 * Copyright: Anki, Inc. 2018
 *
 **/


#include "cannedAnimLib/baseTypes/audioKeyFrameTypes.h"
#include "util/logging/logging.h"
#include "util/math/math.h"
#include "util/random/randomGenerator.h"
#include <limits>

#define ENABLE_AUDIO_PROBABILITY_LOG 0
#define INVALID_EVENT_IDX std::numeric_limits<size_t>::max()

namespace Anki {
namespace Vector {
namespace AudioKeyFrameType {

AudioEventGroupRef& AudioEventGroupRef::operator=( const AudioEventGroupRef& other )
{
  if (&other == this) {
    return *this;
  }
  GameObject = other.GameObject;
  Events = other.Events;
  return *this;
}

bool AudioEventGroupRef::operator==( const AudioEventGroupRef& other ) const 
{
  bool eventsEqual = Events.size() == other.Events.size();
  if(eventsEqual){
    for(int i = 0; i < Events.size(); i++){
      eventsEqual &= (Events[i] == other.Events[i]);
    }
  }
  return eventsEqual &&
         (GameObject == other.GameObject);
}

bool AudioEventGroupRef::EventDef::operator==( const EventDef& other ) const
{
  return (AudioEvent == other.AudioEvent) &&
         (Volume == other.Volume) &&
         (Probability == other.Probability);
}

  
void AudioEventGroupRef::AddEvent( AudioMetaData::GameEvent::GenericEvent audioEvent, float volume, float probability )
{
  Events.emplace_back( audioEvent, volume, probability );
}

const AudioEventGroupRef::EventDef* AudioEventGroupRef::RetrieveEvent( bool useProbability,
                                                                       Util::RandomGenerator* randGen ) const
{
  if ( Events.empty() ) {
    PRINT_NAMED_ERROR("AudioEventGroupRef.EventDef.RetrieveEvent.NoEvents", "");
    return nullptr;
  }
  
  if ( !useProbability || (nullptr == randGen) ) {
    // No probability, return first event
    return &Events.front();
  }
  
  // Taking probabilities into account, select which audio event should be used.
  size_t selectedIdx = INVALID_EVENT_IDX;
  const float randDbl = randGen->RandDbl(1.0);
  float randRangeMin = 0.0f;
  float randRangeMax = 0.0f;
  for (size_t idx = 0; idx < Events.size(); idx++) {
    if (Util::IsFltNear(Events[idx].Probability, 0.0f)) {
      continue;
    }
    randRangeMax = randRangeMin + Events[idx].Probability;
    if (ENABLE_AUDIO_PROBABILITY_LOG) {
      PRINT_CH_DEBUG("Audio", "AudioEventGroupRef.EventDef.RetrieveEvent.ShowInfo",
                     "random value = %f, idx = %i and range = %f to %f",
                     randDbl, (uint32_t)idx, randRangeMin, randRangeMax);
    }
    
    if ( Util::InRange( randDbl, randRangeMin, randRangeMax ) ) {
      // ^ that if statement is equivalent to: if ((randRangeMin <= randDbl) && (randDbl <= randRangeMax))
      selectedIdx = idx;
      break;
    }
    randRangeMin = randRangeMax;
  }
  
  if ( INVALID_EVENT_IDX == selectedIdx ) {
    // Probability has chosen not to play an event
    if (ENABLE_AUDIO_PROBABILITY_LOG) {
      PRINT_CH_DEBUG("Audio", "AudioEventGroupRef.EventDef.RetrieveEvent.InvalidEventIdx",
                     "Event Count: %zi Probability: %f", Events.size(), randDbl);
    }
    return nullptr;
  }

  if (ENABLE_AUDIO_PROBABILITY_LOG) {
    PRINT_CH_DEBUG("Audio", "AudioEventGroupRef.EventDef.RetrieveEvent.RandomAudioSelection",
                   "Probability selected audio index = %ul", (uint32_t)selectedIdx);
  }

  return &Events[selectedIdx];
}


AudioRef::AudioRef( const AudioRef& other )
{
  Tag = other.Tag;
  switch (Tag) {
    case AudioRefTag::EventGroup:
      new (&EventGroup) auto(other.EventGroup);
      break;
    case AudioRefTag::State:
      new (&State) auto(other.State);
      break;
    case AudioRefTag::Switch:
      new (&Switch) auto(other.Switch);
      break;
    case AudioRefTag::Parameter:
      new (&Parameter) auto(other.Parameter);
      break;
  }
}

AudioRef::~AudioRef()
{
  switch (Tag) {
    case AudioRefTag::EventGroup:
      EventGroup.~AudioEventGroupRef();
      break;
    case AudioRefTag::State:
      State.~AudioStateRef();
      break;
    case AudioRefTag::Switch:
      Switch.~AudioSwitchRef();
      break;
    case AudioRefTag::Parameter:
      Parameter.~AudioParameterRef();
      break;
  }
}


AudioRef& AudioRef::operator=( const AudioRef& other )
{
  if (&other == this) {
    return *this;
  }
  Tag = other.Tag;
  switch (Tag) {
    case AudioRefTag::EventGroup:
      EventGroup = other.EventGroup;
      break;
    case AudioRefTag::State:
      State = other.State;
      break;
    case AudioRefTag::Switch:
      Switch = other.Switch;
      break;
    case AudioRefTag::Parameter:
      Parameter = other.Parameter;
      break;
  }
  return *this;
}

bool AudioRef::operator==( const AudioRef& other ) const
{
  bool equal = true;
  equal &= (Tag == other.Tag);
  if(equal){
    switch (Tag) {
      case AudioRefTag::EventGroup:
        equal &= (EventGroup == other.EventGroup);
        break;
      case AudioRefTag::State:
        equal &= (State == other.State);
        break;
      case AudioRefTag::Switch:
        equal &= (Switch == other.Switch);
        break;
      case AudioRefTag::Parameter:
        equal &= (Parameter == other.Parameter);
        break;
    }
  }

  return equal;
}



} // namespace AudioKeyFrameType
} // namespace Vector
} // namespace Anki

