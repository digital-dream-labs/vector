//
//  advertisementService.h
//

#ifndef __ADVERTISEMENT_SERVICE_H__
#define __ADVERTISEMENT_SERVICE_H__

#include <map>
#include "coretech/messaging/shared/UdpServer.h"
#include "clad/types/advertisementTypes.h"

namespace Anki {
  namespace Comms {

    // Receives AdvertisementRegistrationMsg CLAD messages from devices at port(s) that wants to advertise.
    // If enableAdvertisement == 1 and oneShot == 0, the device is registered to the service
    // which will then advertise for the device on subsequent calls to Update().
    // If enableAdvertisement == 1 and oneShot == 1, the service will advertise one time
    // on the next call to Update(). This mode is helpful in that an advertising device
    // need not know whether an advertisement service is running before it sends a registration message.
    // It just keeps sending them!
    // If enableAdvertisement == 0, the device is deregistered if it isn't already.
    //
    // It then tracks and sends on AdvertisementMsg CLAD messages based on the RegistrationMessages
    // to all clients interested in knowing about advertising devices on given port(s).

    class AdvertisementService
    {
    public:
      
      using RegMsgTag = uint8_t;
      static constexpr RegMsgTag kInvalidRegMsgTag = 0;
      
      static const int MAX_SERVICE_NAME_LENGTH = 64;
      
      explicit AdvertisementService(const char* serviceName, RegMsgTag regMsgTag = kInvalidRegMsgTag);
      
      // registrationPort:  Port on which to accept registration messages
      //                    from devices that want to advertise.
      // advertisementPort: Port on which to accept clients that want to
      //                    receive advertisements
      void StartService(int registrationPort, int advertisementPort);
      
      // Stops listening for clients and clears all registered advertisers and advertisement listeners.
      void StopService();
      
      // This needs to be called at the frequency you want to accept registrations and advertise.
      // TODO: Perhaps StartService() should launch a thread to just do this internally.
      void Update();
      
      // Exposed so that you can force-add an advertiser via API
      void ProcessRegistrationMsg(const Vector::AdvertisementRegistrationMsg& msg);

      // Clears the list of advertising devices
      void DeregisterAllAdvertisers();
      
    protected:

      char serviceName_[MAX_SERVICE_NAME_LENGTH];
      
      // Devices that want to advertise connect to this server
      UdpServer regServer_;
      
      // Devices that want to receive advertisements connect to this server
      UdpServer advertisingServer_;
      
      // Map of advertising device id to AdvertisementMsg
      // populated by AdvertisementRegistrationMsg
      using ConnectionInfoMap = std::map<int, Vector::AdvertisementMsg>;
      typedef ConnectionInfoMap::iterator connectionInfoMapIt;
      ConnectionInfoMap connectionInfoMap_;
      
      // Map of advertising device id to AdvertisementMsg
      // for one-shot advertisements, also populated by AdvertisementRegistrationMsg
      ConnectionInfoMap oneShotAdvertiseConnectionInfoMap_;
      
      RegMsgTag _regMsgTag;

    };
    
  }
}

#endif // ADVERTISEMENT_SERVICE_H
