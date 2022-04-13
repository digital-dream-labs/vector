/**
 * File: cozmoFeatureGate
 *
 * Author: baustin
 * Created: 6/15/16
 *
 * Description: Light wrapper for FeatureGate to initialize it with Cozmo-specific configuration
 *
 * Copyright: Anki, Inc. 2016
 *
 **/

#include "engine/utils/cozmoFeatureGate.h"

#include "clad/types/featureGateTypes.h"
#include "coretech/common/engine/utils/data/dataPlatform.h"
#include "engine/cozmoContext.h"
#include "engine/externalInterface/externalMessageRouter.h"
#include "engine/externalInterface/gatewayInterface.h"
#include "json/json.h"
#include "proto/external_interface/messages.pb.h"
#include "util/console/consoleInterface.h"
#include "util/fileUtils/fileUtils.h"
#include "util/logging/logging.h"
#include "util/string/stringUtils.h"
#include "webServerProcess/src/webService.h"

namespace Anki {
namespace Vector {

#define FEATURE_OVERRIDES_ENABLED ( REMOTE_CONSOLE_ENABLED )
  
namespace {
  const std::string kWebVizModuleName = "features";
}

#if FEATURE_OVERRIDES_ENABLED

namespace {
  
  // don't change the order of this, else your overrides will be wrong
  enum FeatureTypeOverride
  {
    Default = 0, // no override
    Enabled, // feature is enabled
    Disabled // feature is disabled
  };

  static FeatureTypeOverride sFeatureTypeOverrides[FeatureTypeNumEntries];
  static std::string sOverrideSaveFilename;

  void PrintFeatureOverrides()
  {
    for ( uint8_t i = 0; i < FeatureTypeNumEntries; ++i )
    {
      const char* status = nullptr;
      switch ( sFeatureTypeOverrides[i] )
      {
        case FeatureTypeOverride::Default:
          continue;

        case FeatureTypeOverride::Enabled:
          status = "Enabled";
          break;

        case FeatureTypeOverride::Disabled:
          status = "Disabled";
          break;

        default:
          break;
      }

      // printing as a warning so it stands out in the console to remind people they have things overridden
      PRINT_NAMED_WARNING( "FeatureGate.Override", "[%s] is %s",
                           EnumToString(static_cast<FeatureType>(i)), status );
    }
  }

  void LoadFeatureOverrides()
  {
    Json::Reader reader;
    Json::Value data;

    const std::string fileContents = Util::FileUtils::ReadFile( sOverrideSaveFilename );
    if ( !fileContents.empty() && reader.parse( fileContents, data ) )
    {
      for ( uint8_t i = 0; i < FeatureTypeNumEntries; ++i )
      {
        const FeatureType feature = static_cast<FeatureType>(i);
        const char* featureString = FeatureTypeToString( feature );

        const Json::Value& overrideTypeJson = data[featureString];
        if ( !overrideTypeJson.isNull() )
        {
          sFeatureTypeOverrides[i] = static_cast<FeatureTypeOverride>(overrideTypeJson.asUInt());
        }
      }

      // if we've loaded something, it means we have overrides, so print them so people are aware they have something set
      PrintFeatureOverrides();
    }
  }

  void SaveFeatureOverrides()
  {
    Json::Value data;

    bool isOverrideSet = false;
    for ( uint8_t i = 0; i < FeatureTypeNumEntries; ++i )
    {
      const FeatureType feature = static_cast<FeatureType>(i);
      const char* featureString = FeatureTypeToString( feature );

      data[featureString] = static_cast<Json::Value::UInt>(sFeatureTypeOverrides[i]);

      isOverrideSet |= ( FeatureTypeOverride::Default != sFeatureTypeOverrides[i] );
    }

    if ( isOverrideSet )
    {
      Util::FileUtils::WriteFile( sOverrideSaveFilename, data.toStyledString() );
    }
    else
    {
      // if we have nothing set, remove the file (since this will be the default state mainly)
      Util::FileUtils::DeleteFile( sOverrideSaveFilename );
    }
  }

  void InitFeatureOverrides( Util::Data::DataPlatform* platform )
  {
    // default everything to "Default", which means no override
    for ( uint8_t i = 0; i < FeatureTypeNumEntries; ++i )
    {
      sFeatureTypeOverrides[i] = FeatureTypeOverride::Default;
    }
    
    const std::string& fileName = "featureGateOverrides.ini";
    sOverrideSaveFilename = platform->pathToResource( Anki::Util::Data::Scope::Cache, fileName );

    LoadFeatureOverrides();
  }

}

static const char* kConsoleFeatureGroup = "FeatureGate";

const char* InitFeatureEnumString() {
  std::vector<std::string> featureNames;
  featureNames.reserve(FeatureTypeNumEntries);
  // Append names of all feature types to console drop-down selection tool
  for ( uint8_t i = 0; i < FeatureTypeNumEntries; ++i )
  {
    featureNames.emplace_back(FeatureTypeToString(static_cast<FeatureType>(i)));
  }
  static const std::string& kFeatureEnumString = Util::StringJoin(featureNames, ',');
  return kFeatureEnumString.c_str();
}
CONSOLE_VAR_ENUM( uint8_t,  kFeatureToEdit,   kConsoleFeatureGroup,  0, InitFeatureEnumString() );

void EnableFeature( ConsoleFunctionContextRef context )
{
  sFeatureTypeOverrides[kFeatureToEdit] = FeatureTypeOverride::Enabled;
  PRINT_NAMED_DEBUG( "FeatureGate.Override", "Enabling feature %s",
                     EnumToString(static_cast<FeatureType>(kFeatureToEdit)) );

  SaveFeatureOverrides();
}
CONSOLE_FUNC( EnableFeature, kConsoleFeatureGroup );

void DisableFeature( ConsoleFunctionContextRef context )
{
  sFeatureTypeOverrides[kFeatureToEdit] = FeatureTypeOverride::Disabled;
  PRINT_NAMED_DEBUG( "FeatureGate.Override", "Disabling feature %s",
                     EnumToString(static_cast<FeatureType>(kFeatureToEdit)) );

  SaveFeatureOverrides();
}
CONSOLE_FUNC( DisableFeature, kConsoleFeatureGroup );

void DefaultFeature( ConsoleFunctionContextRef context )
{
  sFeatureTypeOverrides[kFeatureToEdit] = FeatureTypeOverride::Default;
  PRINT_NAMED_DEBUG( "FeatureGate.Override", "Removing override for feature %s",
                     EnumToString(static_cast<FeatureType>(kFeatureToEdit)) );

  SaveFeatureOverrides();
}
CONSOLE_FUNC( DefaultFeature, kConsoleFeatureGroup );

void DefaultAllFeatures( ConsoleFunctionContextRef context )
{
  for ( uint8_t i = 0; i < FeatureTypeNumEntries; ++i )
  {
    if ( FeatureTypeOverride::Default != sFeatureTypeOverrides[i] )
    {
      sFeatureTypeOverrides[i] = FeatureTypeOverride::Default;
      PRINT_NAMED_DEBUG( "FeatureGate.Override", "Removing override for feature %s",
                        EnumToString(static_cast<FeatureType>(i)) );
    }
  }

  SaveFeatureOverrides();
}
CONSOLE_FUNC( DefaultAllFeatures, kConsoleFeatureGroup );

#endif // FEATURE_OVERRIDES_ENABLED


CozmoFeatureGate::CozmoFeatureGate( Util::Data::DataPlatform* platform )
{
  #if FEATURE_OVERRIDES_ENABLED
  {
    InitFeatureOverrides( platform );
  }
  #endif
  
}

bool CozmoFeatureGate::IsFeatureEnabled(FeatureType feature) const
{

  #if (ANKI_DISABLE_ALEXA)
  if (feature == FeatureType::Alexa) { return false;}
  #endif
  
  #if FEATURE_OVERRIDES_ENABLED
  {
    const uint8_t featureIndex = static_cast<uint8_t>(feature);
    if ( FeatureTypeOverride::Default != sFeatureTypeOverrides[featureIndex] )
    {
      return ( FeatureTypeOverride::Enabled == sFeatureTypeOverrides[featureIndex] );
    }
  }
  #endif

  std::string featureEnumName{FeatureTypeToString(feature)};
  std::transform(featureEnumName.begin(), featureEnumName.end(), featureEnumName.begin(), ::tolower);
  return Util::FeatureGate::IsFeatureEnabled(featureEnumName);
}

void CozmoFeatureGate::SetFeatureEnabled(FeatureType feature, bool enabled)
{
  std::string featureEnumName{FeatureTypeToString(feature)};
  std::transform(featureEnumName.begin(), featureEnumName.end(), featureEnumName.begin(), ::tolower);
  _features[featureEnumName] = enabled;
}
  
void CozmoFeatureGate::Init(const CozmoContext* context, const std::string& jsonContents)
{
  Base::Init( jsonContents );
  
  if( context != nullptr ) {
    // register for app messages requesting feature gates
    auto* gi = context->GetGatewayInterface();
    if( gi != nullptr ) {
      auto handleFeatureFlagRequest = [this, gi](const AnkiEvent<external_interface::GatewayWrapper>& msg) {
        if( msg.GetData().GetTag() == external_interface::GatewayWrapperTag::kFeatureFlagRequest ) {
          const std::string& featureName = msg.GetData().feature_flag_request().feature_name();
          FeatureType featureType = FeatureType::Invalid;
          const bool valid = FeatureTypeFromString( featureName, featureType ) && (featureType != FeatureType::Invalid);
	  
          const bool enabled = valid ? IsFeatureEnabled( featureType ) : false;
          auto* featureFlagResponse = new external_interface::FeatureFlagResponse;
          featureFlagResponse->set_valid_feature( valid );
          featureFlagResponse->set_feature_enabled( enabled );
          gi->Broadcast( ExternalMessageRouter::WrapResponse(featureFlagResponse) );
        } else if( msg.GetData().GetTag() == external_interface::GatewayWrapperTag::kFeatureFlagListRequest ) {
          const bool returnAll = msg.GetData().feature_flag_list_request().request_list().empty();
          // list only those features that are enabled so that an sdk user can't find SuperSecretFeature without brute forcing it
          auto* response = new external_interface::FeatureFlagListResponse;
          response->mutable_list()->Reserve( FeatureTypeNumEntries );
          const auto begin = msg.GetData().feature_flag_list_request().request_list().begin();
          const auto end = msg.GetData().feature_flag_list_request().request_list().end();
          for ( uint8_t i = 0; i < FeatureTypeNumEntries; ++i ) {
            const auto featureType = static_cast<FeatureType>(i);
            if( IsFeatureEnabled( featureType ) ) {
              const bool featureMatches = returnAll || (std::find_if(begin, end, [&](const auto& s) {
                  return Util::StringCaseInsensitiveEquals(EnumToString(featureType), s);
              }) != end);
              if( featureMatches ) {
                auto* feature = response->mutable_list()->Add();
                *feature = EnumToString( featureType );
              }
            }
          }
          gi->Broadcast( ExternalMessageRouter::WrapResponse(response) );
        }
        
      };
      _signalHandles.push_back( gi->Subscribe( external_interface::GatewayWrapperTag::kFeatureFlagRequest, handleFeatureFlagRequest ) );
      _signalHandles.push_back( gi->Subscribe( external_interface::GatewayWrapperTag::kFeatureFlagListRequest, handleFeatureFlagRequest ) );
    }
    
    // register to webviz
    auto* webService = context->GetWebService();
    if (webService != nullptr) {
      
      auto onDataFeatures = [](const Json::Value& data, const std::function<void(const Json::Value&)>& sendToClient) {
        bool success = false;
        #if FEATURE_OVERRIDES_ENABLED
        {
          if( data["type"].isString() && (data["type"].asString() == "reset") )
          {
            {
              for( uint8_t i=0; i<FeatureTypeNumEntries; ++i )
              {
                sFeatureTypeOverrides[i] = Default;
              }
              success = true;
            }
          }
          else if( data["type"].isString() && (data["type"].asString() == "override")
                   && data["name"].isString() && data["override"].isString() )
          {
            FeatureType feature;
            if( FeatureTypeFromString(data["name"].asString(), feature) ) {
              if( data["override"].asString() == "default" ) {
                sFeatureTypeOverrides[static_cast<uint8_t>(feature)] = Default;
                success = true;
              } else if( data["override"].asString() == "enabled" ) {
                sFeatureTypeOverrides[static_cast<uint8_t>(feature)] = Enabled;
                success = true;
              } else if( data["override"].asString() == "disabled" ) {
                sFeatureTypeOverrides[static_cast<uint8_t>(feature)] = Disabled;
                success = true;
              }
            }
          }
          
          if( success ) 
          {
            SaveFeatureOverrides();
          }
        }
        #endif

        if( !success )
        {
          Json::Value error;
          error["error"] = true;
          sendToClient( error );
        }
      };
      
      _signalHandles.emplace_back( webService->OnWebVizData( kWebVizModuleName ).ScopedSubscribe( onDataFeatures ) );
      _signalHandles.emplace_back( webService->OnWebVizSubscribed( kWebVizModuleName ).ScopedSubscribe( std::bind(&CozmoFeatureGate::SendFeaturesToWebViz,
                                                                                                                  this,
                                                                                                                  std::placeholders::_1) ) );
    }
  }
}
  
void CozmoFeatureGate::SendFeaturesToWebViz(const std::function<void(const Json::Value&)>& sendFunc) const
{
  Json::Value data;
  for ( uint8_t i = 0; i < FeatureTypeNumEntries; ++i )
  {
    const auto feature = static_cast<FeatureType>(i);
    Json::Value entry;
    entry["name"] = FeatureTypeToString( feature );
    const std::string featureName = Util::StringToLower( FeatureTypeToString( feature ) );
    entry["default"] = FeatureGate::IsFeatureEnabled( featureName ) ? "enabled" : "disabled";
    #if FEATURE_OVERRIDES_ENABLED
    {
      if( sFeatureTypeOverrides[i] == FeatureTypeOverride::Default )
      {
        entry["override"] = "none";
      }
      else if( sFeatureTypeOverrides[i] == FeatureTypeOverride::Enabled )
      {
        entry["override"] = "enabled";
      }
      else if( sFeatureTypeOverrides[i] == FeatureTypeOverride::Disabled )
      {
        entry["override"] = "disabled";
      }
    }
    # endif
    data.append(entry);
  }
  if( sendFunc ) {
    sendFunc( data );
  }
}

}
}
