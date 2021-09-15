/**
* File: CST_NavMap.cpp
*
* Author: Arjun Menon
* Created: 11-02-2018
*
* Description: 
* Simplified test harness for creating, modifiying, and visualizing navmap
* data and operations for verification and prototyping
*
* Copyright: Anki, inc. 2018
*
*/

#include "simulator/game/cozmoSimTestController.h"
#include "coretech/common/engine/utils/timer.h"
#include "coretech/common/engine/math/polygon.h"
#include "coretech/common/engine/math/bresenhamLine2d.h"
#include "engine/navMap/quadTree/quadTreeTypes.h"
#include "engine/navMap/memoryMap/data/memoryMapData_Cliff.h"
#include "engine/navMap/memoryMap/memoryMap.h"
#include "engine/viz/vizManager.h"
#include "clad/vizInterface/messageViz.h"
#include "clad/externalInterface/messageGameToEngine.h"

namespace Anki {
namespace Vector {

  namespace {
    const f32 kMapRenderRate_sec = 0.25f;

    const f32 kVizPlanarHeight_mm = 1.1f;

    const size_t kReservedBytes = 1 + 2; // Message overhead for:  Tag, and vector size
    const size_t kMaxBufferSize = Anki::Comms::MsgPacket::MAX_SIZE;
    const size_t kMaxBufferForQuads = kMaxBufferSize - kReservedBytes;
    const size_t kFullQuadsPerMessage = kMaxBufferForQuads / sizeof(QuadInfoFullVector::value_type);
  }
  
  // ============ Test class declaration ============
  class CST_NavMap : public CozmoSimTestController
  {
  public:
    
    CST_NavMap();
    
  private:
    
    virtual s32 UpdateSimInternal() override;

    bool _init = false; // whether the Init method is called

    std::unique_ptr<MemoryMap> _map;
    std::unique_ptr<VizManager> _vizm;

    void Init();
    
  };
  
  // Register class with factory
  REGISTER_COZMO_SIM_TEST_CLASS(CST_NavMap);
  
  
  // =========== Test class implementation ===========
  CST_NavMap::CST_NavMap()
  : _init(false)
  , _map(new MemoryMap())
  , _vizm(new VizManager())
  {
  }
  
  s32 CST_NavMap::UpdateSimInternal()
  {
    if(!_init) {
      Init();
      _init = true;
      
      _vizm->EraseSegments("inputLine");
      _vizm->EraseSegments("WO");
      _vizm->EraseSegments("QT");
      _vizm->EraseSegments("test_rays");

      const auto& quadTree = _map->_quadTree;

      MemoryMapData_Cliff initData(Pose3d(), 0);
      initData.isFromCliffSensor = true;

      const auto p1 = Point3f(200,0,0);
      const auto p2 = Point3f(0,150,0);
      _map->Insert(FastPolygon({p1,p2}), initData);
      _vizm->DrawSegment("inputLine", p1, p2, NamedColors::BLACK, true, kVizPlanarHeight_mm);

      // printout some transformation between integral and cartesian for checking
      const f32 precision = quadTree.GetContentPrecisionMM();
      const uint8_t height = quadTree.GetMaxHeight();
      const Point2f center = quadTree.GetCenter();
      auto integralP1 = GetIntegralCoordinateOfNode(p1, center, precision, height);
      auto integralP2 = GetIntegralCoordinateOfNode(p2, center, precision, height);
      auto reprojP1 = GetCartesianCoordinateOfNode(integralP1, center, precision, height);
      auto reprojP2 = GetCartesianCoordinateOfNode(integralP2, center, precision, height);
      auto addrP1 = GetAddressForNodeCenter(integralP1, height);
      auto addrP2 = GetAddressForNodeCenter(integralP2, height);

      PRINT_NAMED_INFO("CST_NavMap", "P1 %11s coordinate = %s", "input", p1.ToString().c_str());
      PRINT_NAMED_INFO("CST_NavMap", "P1 %11s coordinate = %s", "integral", integralP1.ToString().c_str());
      PRINT_NAMED_INFO("CST_NavMap", "P1 %11s coordinate = %s", "reprojected", reprojP1.ToString().c_str());
      PRINT_NAMED_INFO("CST_NavMap", "P1 address = %s", ToString(addrP1).c_str());

      PRINT_NAMED_INFO("CST_NavMap", "P2 %11s coordinate = %s", "input", p2.ToString().c_str());
      PRINT_NAMED_INFO("CST_NavMap", "P2 %11s coordinate = %s", "integral", integralP2.ToString().c_str());
      PRINT_NAMED_INFO("CST_NavMap", "P2 %11s coordinate = %s", "reprojected", reprojP2.ToString().c_str());
      PRINT_NAMED_INFO("CST_NavMap", "P2 address = %s", ToString(addrP2).c_str());

      PRINT_NAMED_INFO("CST_NavMap", "Max Tree Height = %d", (int)height);

      // generate the rays emanating from the robot position, sweeping some degrees 
      // around the recently inserted line in the NavMap
      std::vector<Point2f> testRayPoints;
      const f32 radius = 300;
      for(int i=0; i<=300; ++i) {
        f32 angle = -M_PI_4_F + (i*M_PI_F/300);
        f32 testX = radius * std::cos(angle);
        f32 testY = radius * std::sin(angle);
        testRayPoints.push_back({testX, testY});
      }

      // visualizing a single ray
      #if (0)
      {
        const auto getPoint3 = [&](const Point2f& p) -> Point3f {
          return {p.x(), p.y(), kVizPlanarHeight_mm};
        };
        int rayIdx = 70;
        int quadCount = 0;

        // draw the rasterized nodes:
        auto rayStart = QuadTreeTypes::GetIntegralCoordinateOfNode({0.f,0.f}, center, precision, height);
        auto rayEnd = QuadTreeTypes::GetIntegralCoordinateOfNode(testRayPoints[rayIdx], center, precision, height);
        auto bresPoints = GetBresenhamLine(rayStart, rayEnd, true);

        PRINT_NAMED_INFO("CST_NavMap", "start coordinate = (%s)", rayStart.ToString().c_str());
        PRINT_NAMED_INFO("CST_NavMap", "final coordinate = (%s)", rayEnd.ToString().c_str());
        PRINT_NAMED_INFO("CST_NavMap", "num raster points = %zd", bresPoints.size());

        auto projRayStart = GetCartesianCoordinateOfNode(rayStart, center, precision, height);
        auto projRayEnd = GetCartesianCoordinateOfNode(rayEnd, center, precision, height);
        _vizm->DrawFrameAxes("start_point", Pose3d(0.0, Z_AXIS_3D(), getPoint3(projRayStart)), 30);
        _vizm->DrawFrameAxes("final_point", Pose3d(0.0, Z_AXIS_3D(), getPoint3(projRayEnd)), 30);

        bool toggle = false;
        for(int i=1; i<bresPoints.size(); ++i) {
          _vizm->DrawSegment("bresenhamLine", 
            getPoint3(GetCartesianCoordinateOfNode(bresPoints[i-1], center, precision, height)), 
            getPoint3(GetCartesianCoordinateOfNode(bresPoints[i], center, precision, height)), 
            toggle ? NamedColors::RED : NamedColors::BLUE, 
            i==1, kVizPlanarHeight_mm);
          toggle = !toggle;
        }
      
        for(auto& p : bresPoints) {
          auto testAddr = GetAddressForNodeCenter(p, height);
          PRINT_NAMED_INFO("CST_NavMap", "NodeAddress for p(%s) = %s", p.ToString().c_str(), ToString(testAddr).c_str());
          const auto qtn = quadTree.GetNodeAtAddress(testAddr);
          if(qtn == nullptr) { 
            continue; 
          }
          auto cen = qtn->GetCenter();
          Pose3d pose = Pose3d(0.f, Z_AXIS_3D(), {cen.x(), cen.y(), 0.f});
          auto side = qtn->GetSideLen()/2;
          Quad2f cellQuad {
            {-side, side},
            {-side,-side},
            { side, side},
            { side,-side},
          };
          ((Pose2d)pose).ApplyTo(cellQuad, cellQuad);
          auto cellCenter = QuadTreeTypes::GetCartesianCoordinateOfNode(p, center, precision, height);
          _vizm->DrawQuad(VizQuadType::VIZ_QUAD_GENERIC_2D, quadCount++, cellQuad, kVizPlanarHeight_mm, NamedColors::GREEN);
          _vizm->DrawFrameAxes("testFrame", Pose3d(0.f, Z_AXIS_3D(), getPoint3(cellCenter)), 20);
        }
      
        _vizm->DrawSegment( "testRay", 
                            Point3f{0.f, 0.f, 0.f}, 
                            Point3f{testRayPoints[rayIdx].x(), testRayPoints[rayIdx].y(), 0.f}, 
                            NamedColors::BLACK, 
                            true, 
                            kVizPlanarHeight_mm);
      }
      #endif

      std::vector<double> singleRayCheckTimes;
      NodePredicate collisionCheckFun = [](MemoryMapDataConstPtr in)->bool {
        return in->type == MemoryMapTypes::EContentType::Cliff;
      };

      
      const auto start_memo = std::chrono::system_clock::now();
      std::vector<bool> collisionCheckResults = _map->AnyOf({0.f, 0.f}, testRayPoints, collisionCheckFun);
      const auto memo_time_us = double((std::chrono::system_clock::now() - start_memo).count()) / testRayPoints.size();

      size_t countMismatch = 0;
      for(int rayIdx=0; rayIdx<=300; ++rayIdx) {
        const auto start = std::chrono::system_clock::now();
        bool inCollision1Ray = _map->AnyOf(FastPolygon({{0.f, 0.f}, testRayPoints[rayIdx]}), collisionCheckFun);
        const auto basic_time_us = (std::chrono::system_clock::now() - start).count();
        singleRayCheckTimes.push_back(basic_time_us);

        // draw a fan of rays, red if it collides, blue if it doesn't
        // _vizm->DrawSegment("test_rays", 
        //                    Point3f{0.f, 0.f, 0.f}, 
        //                    Point3f{testRayPoints[i].x(), testRayPoints[i].y(), 0.f}, 
        //                    collisionCheckResults[i] ? NamedColors::RED : NamedColors::BLUE, 
        //                    false, kVizPlanarHeight_mm);

        if(collisionCheckResults[rayIdx] != inCollision1Ray) {
          PRINT_NAMED_INFO("CST_NavMap", 
                           "Collision Results do not match (%d) (got=%d exp=%d)", 
                           rayIdx, (int)collisionCheckResults[rayIdx], (int)inCollision1Ray);
          countMismatch++;
        }
      }


      // due to rounding and other precision-based errors, not all rays will
      // provide the same results for collision free or not
      // thus we allow for a margin of getting two rays wrong (1 per side of
      // the inserted cliff region), as the upper limit of rays that are wrong
      CST_EXPECT( countMismatch <= 2, "Got too many rays incorrect" );

      double single_time_us = 0.0;
      for(int i=0; i<singleRayCheckTimes.size(); ++i) {
        single_time_us += singleRayCheckTimes[i];
      }
      single_time_us /= singleRayCheckTimes.size();

      PRINT_NAMED_INFO("CST_NavMap", "Normal = %6.6f, Memo+Raster = %6.6f", single_time_us, memo_time_us);
      CST_EXPECT( FLT_LE(memo_time_us, single_time_us), "Ray Checking slower than checking single rays.");
    }

    const f32 currentTime_s = BaseStationTimer::getInstance()->GetCurrentTimeInSeconds();
    static f32 nextDrawTime_s = currentTime_s;
    const bool doViz = FLT_LE(nextDrawTime_s, currentTime_s);
    if( doViz ) {
      // Reset the timer but don't accumulate error
      nextDrawTime_s += ((int) (currentTime_s - nextDrawTime_s) / kMapRenderRate_sec + 1) * kMapRenderRate_sec;
      MemoryMapTypes::MapBroadcastData data;
      _map->GetBroadcastInfo(data);

      using namespace VizInterface;
      _vizm->SendVizMessage(MessageViz(MemoryMapMessageVizBegin(0, data.mapInfo)));
      for(u32 seqNum = 0; seqNum*kFullQuadsPerMessage < data.quadInfoFull.size(); seqNum++)
      {
        auto start = seqNum*kFullQuadsPerMessage;
        auto end   = std::min(data.quadInfoFull.size(), start + kFullQuadsPerMessage);
        _vizm->SendVizMessage(MessageViz(MemoryMapMessageViz(0, QuadInfoFullVector(data.quadInfoFull.begin() + start, data.quadInfoFull.begin() + end))));
      }
      _vizm->SendVizMessage(MessageViz(MemoryMapMessageVizEnd(0)));
    }

    CST_EXIT();
    return _result;
  }

  void CST_NavMap::Init()
  {
    // turn off robot's copy of the map
    ExternalInterface::SetMemoryMapRenderEnabled m;
    m.enabled = false;
    ExternalInterface::MessageGameToEngine message(std::move(m));
    UiGameController::SendMessage(message);

    // connect the simtest's viz manager to the physVizController
    _vizm->Connect("127.0.0.1", (uint16_t)VizConstants::VIZ_SERVER_PORT);
  }
  
} // end namespace Vector
} // end namespace Anki

