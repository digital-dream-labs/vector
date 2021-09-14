## 6/16/2018 notes richard@anki.com

# MP - MicData performance

  - When idle, trigger processing is 36%, dropping to 24% when active, raw processing remains about the same

## Idle, on charger

#### THREAD: vic-anim
```
24.4%      vic-anim
  66.9%      vic-anim
    48.0%      ProceduralFaceDrawer
  16.4%      libc
   8.9%      libopencv
   7.8%      libc++
```

#### THREAD: MicProcTrigger
```
36.9%      MicProcTrigger
  98.2%      vic-anim
    80.2%      u139y
    10.0%      b383i
     4.0%      eb41l
   1.8%      libpthread
```

#### THREAD: MicProcRaw
```
30.4%      MicProcRaw
  85.0%      vic-anim
    10.3%      DoSpatialFilter
     8.6%      .loop_8_taps
     7.8%      .i_loop_no_load
     6.9%      VGenComplextone_i16_ansi
     6.4%      VAdd_i16_ani
     5.8%      DoBeamDecision
     4.2%      rftfsub
     4.1%      rad4_ps
     3.6%      MapRatioToFuzzy
  10.3%      libc
    82.2%      memcpy
    17.8%      libc
   4.7%      libm
    52.5%      __pow_finite
    47.8%      __atan2f_finite
```

#### THREAD: WWISE
```
 6.9%      vic-anim
```

## Active

#### THREAD: MicProcRaw
```
32.9%      MicProcRaw (>30.4%)
  85.0%      vic-anim
    13.4%      .i_loop_no_load (>7.8%)
     8.1%      .loop_8 (+)
     6.4%      VGenComplextone_i16_ansi (=6.9%)
     0.4%      MapRatioToFuzzy (<3.6%)
     3.6%      DoSpatialFilter (<10.3%)
     3.0%      .loop_8_taps (<8.6%)
     2.8%      rad4_ps (<4.1%)
     0.8%      VAdd_i16_ansi (<6.4%)
     0.7%      DoBeamDecision (<5.8%)
     0.2%      rftfsub (<4.2%)
  11.4%      libc (=10.3%)
    82.2%      memcpy
    17.8%      libc
   4.2%      libm (4.7%)
    30.8%      atanf
    19.3%      __log_finite
    16.0%      __atan2f_finite (<47.8%)
    10.8%      __pow_finite (<52.5%)
```

#### THREAD: MicProcTrigger
```
24.4%      MicProcTrigger (<36.9%)
  98.3%      vic-anim (=98.2%)
    87.7%      u139y (>80.2%)
     3.7%      w3e8m
     3.5%      b383i (<10.0%)
     1.4%      g597v.constprop.1
     1.0%      eb41l (<4.0%)
   1.8%      libpthread
```

#### THREAD: Animation Streamer
```
24.4%      vic-anim (<28.0%)
  66.9%      vic-anim
    48.0%      ProceduralFaceDrawer
  16.4%      libc
   8.9%      libopencv
   7.8%      libc++
```

#### THREAD: WWISE
```
 6.9%      vic-anim (>12.8%)
```

## 6/12/2018 notes richard@anki.com

# DVT3 - MicData performance

  - No degradation in performance since 5/8/2018
  - Little difference in process between the default voice and smaller-sized voice when idle.
  - When idle, trigger processing is <7% jumping to 27% when active then raw processing jumps from 18% to 56%

## Idle, on charger

```
DEFAULT VOICE                           LITTLE VOICE
```

#### THREAD: vic-anim
```
61%        vic-anim                      59%        vic-anim
```

#### THREAD: MicProcRaw
```
18.7%      MicProcRaw                    21%        MicProcRaw
  61.2%      vic-anim                      66%        vic-anim
                                             7.4%       .i_loop_no_load

    5.7%       aubio_fft_get_norm            4.4%       aubio_fft_get_norm
    5.6%       ProcessRawAudio               4.3%       ProcessRawAudio
    4.1%       cfdmdl                        3.7%       cfdmdl
    3.6%       DcRemovalFilter_f32           3.0%       DcRemovalFilter_f32
    3.6%       fvec_weight                   2.7%       fvec_weight
    3.3%       AddSamples                    3.8%       AddSamples
    3.2%       plt                           2.7%       plt
    3.2%       cft1st                        3.2%       cft1st
    3.2%       _loop8                        1.3%       _loop8
    2.3%       ProcessMicrophonesSE          2.1%       ProcessMicrophonesSE
    2.3%       AnkiStatsAccumulator          2.1%       AnkiStatsAccumulator
    1.6%       ProcessRawLoop                1.7%       ProcessRawLoop

  17%        libm                          13.92%     libm
    61%        atanf                         51.6%      atanf
    23%        _atan2f_finite                27.7%      _atan2f_finite
    13%        atan2f                        14%        atan2f
                                              3.1%      __log_finite
  14%        libc                          12.5%      libc
    36%        memcpy                        50.14%     memcpy
    11.9%      __clock_gettime               15.5%      __clock_gettime
```

#### THREAD: wwise
```
 9.6%      vic-anim (wwise)                10%       vic-anim (wwise)
```

#### THREAD: MicProcTrigger
```
 7.5%      MicProcTrigger                 6.4%      MicProcTrigger
  82%        vic-anim                       78%       vic-anim
    78.8%      u139y                          63%       u139y
                                               6.7%     w3e8m
     7.1%      ProcessTriggerLoop              5.9%     ProcessTriggerLoop
     2.9%      b383i                           3.3%     b383i

   5.8%      libpthread                      7.8%     libpthread
   5.1%      libc                            4.5%     libc
     32.2%     memcpy                          
   4%        libc++                          4.2%     libc++
```

#### THREAD: wwise
```
 1.3%      vic-anim (wwise)               1.2%      vic-anim (wwise)
```

## Active

```
DEFAULT VOICE TALKING
```

#### THREAD: vic-anim
```
25%      vic-anim
```

#### THREAD: MicProcRaw
```
46%      MicProcRaw
  81%      vic-anim
    19%      .i_loop_no_load
     9.3%    ConvertFloatToInt16
     1.7%    ProcessRawAudio
     5.6%    .qmask_add_pc
     5.5%    .vcmul_quad_loop_start
     1.6%    DcRemovalFilter_f32
     5.9%    AddSamples
     3.4%    AnkiStatsAccumulator

  6.2%     libm
    66.6%    atanf
    33.4%    __log_finite

  10.3%    libc
```

#### THREAD: wwise
```
      vic-anim (wwise)
```

#### THREAD: MicProcTrigger
```
27%      MicProcTrigger
  100%     vic-anim
    89.6%    u139y
     7%      b383i
     3.5%    w3e8m
```

## 5/8/2018 notes richard@anki.com

# DVT2 - CPU performance

Note: idle means sitting on the charger for 2 minutes, shaken is free-play plus spoken at for 2 minutes

  - CLAHE_Interpolation_Body functions are gone
  - MicData is still more expensive than face rendering
  - 38% of face rendering is noise, 35% converting to HSV, 17% drawing eyes
  - malloc and new in the results
  - Dev_AssertIsValidParentPointer is still in the results
  - vic-cloud does not show results

## PROCESS: vic-engine

### Top 20 functions idle

```
Overhead  Command          Tid    Shared Object             Symbol
16.77%    VisionSystem     8761   libcozmo_engine           void Anki::Embedded::ScrollingIntegralImage_u8_s32::FilterRow_innerLoop<unsigned char>(int, int, int, int, int const*, int const*, int const*, int const*, unsigned char*)
5.64%     VisionSystem     8761   libcozmo_engine           Anki::Vision::ImageRGB::FillGray(Anki::Vision::Image&) const
4.26%     VisionSystem     8761   libcozmo_engine           Anki::Embedded::ConnectedComponentsTemplate<unsigned short>::Extract2dComponents_PerRow_NextRow(unsigned char const*, int, short, short, short)
4.12%     VisionSystem     8761   libcozmo_engine           Anki::Embedded::ScrollingIntegralImage_u8_s32::ScrollDown(Anki::Embedded::Array<unsigned char> const&, int, Anki::Embedded::MemoryStack)
3.79%     VisionSystem     8761   libcozmo_engine           Anki::Embedded::ConnectedComponentsTemplate<unsigned short>::Extract1dComponents(unsigned char const*, short, short, short, Anki::Embedded::FixedLengthList<Anki::Embedded::ConnectedComponentSegment<unsigned short> >&)
2.72%     VisionSystem     8761   libcozmo_engine           Anki::Embedded::ecvcs_computeBinaryImage_numFilters3(Anki::Embedded::Array<unsigned char> const&, Anki::Embedded::FixedLengthList<Anki::Embedded::Array<unsigned char> >&, int, int, unsigned char*, bool)
1.86%     CozmoRunner      8698   libcozmo_engine           Anki::PoseBase<Anki::Pose3d, Anki::Transform3d>::PoseTreeNode::Dev_AssertIsValidParentPointer(Anki::PoseBase<Anki::Pose3d, Anki::Transform3d>::PoseTreeNode const*, Anki::PoseBase<Anki::Pose3d, Anki::Transform3d>::PoseTreeNode const*)
1.52%     CozmoRunner      8698   libcozmo_engine           Anki::Cozmo::QuadTreeNode::Intersects(Anki::FastPolygon const&) const
1.47%     CozmoRunner      8698   libc                      malloc
1.46%     CozmoRunner      8698   libcozmo_engine           std::__1::__function::__func<Anki::Cozmo::QuadTree::Transform(std::__1::function<Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> > (Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> >)>)::$_5, std::__1::allocator<Anki::Cozmo::QuadTree::Transform(std::__1::function<Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> > (Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> >)>)::$_5>, void (Anki::Cozmo::QuadTreeNode&)>::operator()(Anki::Cozmo::QuadTreeNode&)
1.34%     CozmoRunner      8698   libcozmo_engine           std::__1::__function::__func<Anki::Cozmo::MemoryMap::GetBroadcastInfo(Anki::Cozmo::MemoryMapTypes::MapBroadcastData&) const::$_12, std::__1::allocator<Anki::Cozmo::MemoryMap::GetBroadcastInfo(Anki::Cozmo::MemoryMapTypes::MapBroadcastData&) const::$_12>, void (Anki::Cozmo::QuadTreeNode const&)>::operator()(Anki::Cozmo::QuadTreeNode const&)
1.28%     CozmoRunner      8698   libcozmo_engine           std::__1::enable_if<std::is_base_of<Anki::ConvexPolygon, Anki::FastPolygon>::value, void>::type Anki::Cozmo::QuadTreeNode::Fold<Anki::FastPolygon>(std::__1::function<void (Anki::Cozmo::QuadTreeNode&)>, Anki::FastPolygon const&, Anki::Cozmo::QuadTreeTypes::FoldDirection)
1.10%     VisionSystem     8761   libcozmo_engine           Anki::Cozmo::RollingShutterCorrector::ComputePixelShiftsWithImageIMU(unsigned int, Anki::Point<2u, float>&, Anki::Cozmo::VisionPoseData const&, Anki::Cozmo::VisionPoseData const&, float)
1.08%     CozmoRunner      8698   libcozmo_engine           Anki::Cozmo::QuadTreeNode::Fold(std::__1::function<void (Anki::Cozmo::QuadTreeNode&)>, Anki::Cozmo::QuadTreeTypes::FoldDirection)
1.07%     VisionSystem     8761   libc                      memset
0.98%     CozmoRunner      8698   libpthread-2.22           pthread_mutex_lock
0.93%     CozmoRunner      8698   libcozmo_engine           std::__1::__function::__func<Anki::Cozmo::QuadTree::Insert(Anki::FastPolygon const&, std::__1::function<Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> > (Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> >)>)::$_1, std::__1::allocator<Anki::Cozmo::QuadTree::Insert(Anki::FastPolygon const&, std::__1::function<Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> > (Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> >)>)::$_1>, void (Anki::Cozmo::QuadTreeNode&)>::operator()(Anki::Cozmo::QuadTreeNode&)
0.92%     VisionSystem     8761   libc                      __clock_gettime
0.82%     CozmoRunner      8698   libcozmo_engine           @plt
0.77%     CozmoRunner      8698   libcozmo_engine           Anki::FastPolygon::Contains(float, float) const
```

### Top 20 functions shaken

```
Overhead  Command          Tid    Shared Object             Symbol
11.40%    FaceRecognizer   8748   libcozmo_engine           OMR_F_FR80_0042
10.55%    VisionSystem     8761   libcozmo_engine           void Anki::Embedded::ScrollingIntegralImage_u8_s32::FilterRow_innerLoop<unsigned char>(int, int, int, int, int const*, int const*, int const*, int const*, unsigned char*)
3.33%     FaceRecognizer   8748   libcozmo_engine           OMR_F_FR80_0049
2.78%     CozmoRunner      8698   libcozmo_engine           std::__1::__function::__func<Anki::Cozmo::QuadTree::Transform(std::__1::function<Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> > (Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> >)>)::$_5, std::__1::allocator<Anki::Cozmo::QuadTree::Transform(std::__1::function<Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> > (Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> >)>)::$_5>, void (Anki::Cozmo::QuadTreeNode&)>::operator()(Anki::Cozmo::QuadTreeNode&)
2.73%     VisionSystem     8761   libcozmo_engine           OMR_F_DT_0031
2.72%     VisionSystem     8761   libcozmo_engine           Anki::Embedded::ScrollingIntegralImage_u8_s32::ScrollDown(Anki::Embedded::Array<unsigned char> const&, int, Anki::Embedded::MemoryStack)
2.66%     CozmoRunner      8698   libcozmo_engine           Anki::Cozmo::QuadTreeNode::Fold(std::__1::function<void (Anki::Cozmo::QuadTreeNode&)>, Anki::Cozmo::QuadTreeTypes::FoldDirection)
2.47%     VisionSystem     8761   libcozmo_engine           OMR_F_PT50_0003
2.37%     CozmoRunner      8698   libcozmo_engine           std::__1::__function::__func<Anki::Cozmo::MemoryMap::GetBroadcastInfo(Anki::Cozmo::MemoryMapTypes::MapBroadcastData&) const::$_12, std::__1::allocator<Anki::Cozmo::MemoryMap::GetBroadcastInfo(Anki::Cozmo::MemoryMapTypes::MapBroadcastData&) const::$_12>, void (Anki::Cozmo::QuadTreeNode const&)>::operator()(Anki::Cozmo::QuadTreeNode const&)
2.11%     FaceRecognizer   8748   libcozmo_engine           OMR_F_FR80_0056
2.11%     VisionSystem     8761   libcozmo_engine           Anki::Embedded::ConnectedComponentsTemplate<unsigned short>::Extract1dComponents(unsigned char const*, short, short, short, Anki::Embedded::FixedLengthList<Anki::Embedded::ConnectedComponentSegment<unsigned short> >&)
1.68%     VisionSystem     8761   libcozmo_engine           Anki::Embedded::ecvcs_computeBinaryImage_numFilters3(Anki::Embedded::Array<unsigned char> const&, Anki::Embedded::FixedLengthList<Anki::Embedded::Array<unsigned char> >&, int, int, unsigned char*, bool)
1.47%     VisionSystem     8761   libopencv_imgproc         cv::ColumnSum<int, unsigned char>::operator()(unsigned char const**, unsigned char*, int, int, int)
1.24%     VisionSystem     8761   libcozmo_engine           Anki::Embedded::ConnectedComponentsTemplate<unsigned short>::Extract2dComponents_PerRow_NextRow(unsigned char const*, int, short, short, short)
1.09%     FaceRecognizer   8748   libcozmo_engine           OMR_F_FR80_0054
1.06%     VisionSystem     8761   libcozmo_engine           OMR_F_DT_0048
1.04%     VisionSystem     8761   libopencv_imgproc         cv::resizeNNInvoker::operator()(cv::Range const&) const
1.03%     VisionSystem     8761   libcozmo_engine           Anki::Vision::ImageRGB::FillGray(Anki::Vision::Image&) const
1.03%     CozmoRunner      8698   libcozmo_engine           std::__1::__function::__func<Anki::Cozmo::MapComponent::TimeoutObjects()::$_1, std::__1::allocator<Anki::Cozmo::MapComponent::TimeoutObjects()::$_1>, Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> > (Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> >)>::operator()(Anki::Cozmo::MemoryMapDataWrapper<Anki::Cozmo::MemoryMapData, std::__1::enable_if<true, void> >&&)
0.99%     FaceRecognizer   8748   libcozmo_engine           OMR_F_FR80_0041
```

### Hierarchical idle

#### THREAD: VisionSystem (51%)
```
  88% libcozmo_engine
    37% ScrollingIntegralImage_u8_s32::FilterRow_innerLoop
    12% FillGray
     9% ConnectedComponentsTemplate::Extract2dComponents_PerRow_NextRow
     9% ScrollingIntegralImage_u8_s32::ScrollDown
     8% ConnectedComponentsTemplate::Extract1dComponents_PerRow_NextRow
```

#### THREAD: CozmoRunner (47%)
```
  62% cozmo_engine
    17% other
    7% Dev_AssertDevIsValidParentPointer
    6% QuadTreeNode::Intersects
    5% QuadTreeNode::Transform
    5% MemoryMap::GetBroadcastInfo

  18% libc
    19% malloc
     6% free

   8% libc++
     8% new
     3% delete
```

### Hierarchical shaken

#### THREAD: VisionSystem (41% -10%)
```
  88% libcozmo_engine
    37% ScrollingIntegralImage_u8_s32::FilterRow_innerLoop
    12% FillGray
     9% ConnectedComponentsTemplate::Extract2dComponents_PerRow_NextRow
     9% ScrollingIntegralImage_u8_s32::ScrollDown
     8% ConnectedComponentsTemplate::Extract1dComponents_PerRow_NextRow
```

#### THREAD: CozmoRunner (37% -10%)
```
  65% cozmo_engine (+3%)
    17% other (=)
    14% QuadTreeNode::Transform (+9%)
    12% MemoryMap::GetBroadcastInfo (+9%)
    5% MapComponent::TimeoutObjects (NEW)
    4% Dev_AssertDevIsValidParentPointer (-3%)

  17% libc (-1%)
    17% malloc (-2%)
     7% free (+1%)
     7% memcpy
     5% vfprintf

  10% libc++ (+2%)
     4% new (-4%)
     2% delete (-1%)
```

#### THREAD: FaceRecognizer (20%)
```
  100% OMR_F_*
```

## PROCESS: vic-anim

### Top 20 functions idle

```
Overhead  Command          Tid    Shared Object             Symbol
36.70%    MicProcTrigger   8707   vic-anim                  u139y
6.12%     MicProcRaw       8706   vic-anim                  .i_loop_no_load
6.06%     vic-anim         8678   vic-anim                  Anki::Cozmo::ProceduralFaceDrawer::ApplyNoise(Anki::Util::RandomGenerator const&, Anki::Vision::ImageRGB565 const&, Anki::Vision::ImageRGB565&, Anki::Cozmo::ProceduralFaceDrawer::DrawCacheState)
5.58%     vic-anim         8678   vic-anim                  Anki::Vision::ImageRGB::ConvertHSV2RGB565(Anki::Vision::ImageRGB565&)
2.71%     MicProcRaw       8706   vic-anim                  VGenComplexTone_i16_ansi
2.63%     vic-anim         8678   vic-anim                  Anki::Cozmo::ProceduralFaceDrawer::DrawEye(Anki::Cozmo::ProceduralFace const&, Anki::Cozmo::ProceduralFace::WhichEye, Anki::Vision::ImageRGB&, Anki::Rectangle<float>&)
2.05%     vic-anim         8678   libopencv_imgproc         void cv::remapNearest<unsigned char>(cv::Mat const&, cv::Mat&, cv::Mat const&, int, cv::Scalar_<double> const&)
1.96%     MicProcRaw       8706   libc                      memcpy
1.65%     MicProcTrigger   8707   vic-anim                  w3e8m
1.53%     MicProcTrigger   8707   vic-anim                  b383i
1.36%     MicProcRaw       8706   vic-anim                  ConvertFloatToInt16
1.13%     MicProcRaw       8706   vic-anim                  radf4_ps
1.09%     vic-anim         8678   libm                      roundf
0.99%     MicProcRaw       8706   vic-anim                  .qmask_add_pc
0.93%     MicProcRaw       8706   vic-anim                  .vcmul_quad_loop_start
0.91%     MicProcRaw       8706   vic-anim                  .vcrmul_quad_loop_start
0.91%     MicProcRaw       8706   vic-anim                  .loop_8_taps
0.83%     vic-anim         8678   libc                      memcpy
0.80%     MicProcRaw       8706   vic-anim                  VDotProductsLeftShift_q15_i16_ansi
```

### Top 20 functions shaken

```
Overhead  Command          Tid    Shared Object             Symbol
33.55%    MicProcTrigger   8707   vic-anim                  u139y
5.71%     MicProcRaw       8706   vic-anim                  .i_loop_no_load
4.90%     vic-anim         8678   vic-anim                  Anki::Cozmo::ProceduralFaceDrawer::ApplyNoise(Anki::Util::RandomGenerator const&, Anki::Vision::ImageRGB565 const&, Anki::Vision::ImageRGB565&, Anki::Cozmo::ProceduralFaceDrawer::DrawCacheState)
4.58%     vic-anim         8678   vic-anim                  Anki::Vision::ImageRGB::ConvertHSV2RGB565(Anki::Vision::ImageRGB565&)
2.81%     MicProcRaw       8706   vic-anim                  .loop_8
2.41%     MicProcRaw       8706   vic-anim                  VGenComplexTone_i16_ansi
2.19%     vic-anim         8678   libopencv_imgproc         void cv::remapNearest<unsigned char>(cv::Mat const&, cv::Mat&, cv::Mat const&, int, cv::Scalar_<double> const&)
2.19%     vic-anim         8678   vic-anim                  Anki::Cozmo::ProceduralFaceDrawer::DrawEye(Anki::Cozmo::ProceduralFace const&, Anki::Cozmo::ProceduralFace::WhichEye, Anki::Vision::ImageRGB&, Anki::Rectangle<float>&)
1.76%     MicProcRaw       8706   libc                      memcpy
1.49%     MicProcTrigger   8707   vic-anim                  b383i
1.33%     MicProcTrigger   8707   vic-anim                  w3e8m
1.14%     MicProcRaw       8706   vic-anim                  ConvertFloatToInt16
1.09%     MicProcRaw       8706   vic-anim                  .qmask_add_pc
1.06%     MicProcRaw       8706   vic-anim                  radf4_ps
0.99%     MicProcRaw       8706   vic-anim                  .loop_8_taps
0.98%     vic-anim         8678   libm                      roundf
0.95%     MicProcRaw       8706   vic-anim                  .load_qmask
0.86%     MicProcRaw       8706   vic-anim                  .vcrmul_quad_loop_start
0.84%     MicProcRaw       8706   vic-anim                  .vcmul_quad_loop_start
0.82%     MicProcRaw       8706   vic-anim                  VDotProductsLeftShift_q15_i16_ansi
```

### Hierarchical idle

#### THREAD: MicProcTrigger (42%)
```
86% u139y
 4% w3e8m
```

#### THREAD: MicProcRaw (31%)
```
22% .i_loop_no_load
10% VGenComplextone_i16_ansi
```

#### THREAD: vic-anim (25%)
```
63% vic-anim
  38% ApplyNoise
  35% ConvertHSV
  17% DrawEye

15% opencv

 8% libc
```

### Hierarchical shaken

#### THREAD: MicProcTrigger (38% -3%)
```
88% u139y (+2%)
 4% w3e8m (=)
```

#### THREAD: MicProcRaw (32% ==)
```
89% vic-anim
  20% .i_loop_no_load (-2%)
  10% .loop_8 (NEW)
   8% VGenComplextone_i16_ansi (-2%)

 7% libc
   80% memcpy
```

#### THREAD: vic-anim (22% -3%)
```
60% vic-anim (-3%)
  37% ApplyNoise (-1%)
  35% ConvertHSV (=)
  17% DrawEye (=)

17% opencv (+2%)

 9% libc (+1%)
```

## PROCESS: vic-cloud

### Top 20 functions idle

```
Overhead  Command          Tid    Shared Object             Symbol
49.00%    vic-cloud        8684   vic-cloud                 vic-cloud[+5fde0]
25.93%    vic-cloud        8684   vic-cloud                 _start
19.77%    vic-cloud        8684   [vectors]                 [vectors][+fc8]
4.42%     vic-cloud        8684   [vectors]                 [vectors][+fc0]
0.84%     vic-cloud        8684   [vectors]                 [vectors][+fa0]
0.03%     vic-cloud        8684   vic-cloud                 vic-cloud[+5fe30]
```

### Top 20 functions shaken

```
Overhead  Command          Tid    Shared Object             Symbol
89.50%    vic-cloud        8684   vic-cloud                 _start
8.68%     vic-cloud        8684   [vectors]                 [vectors][+fd8]
1.80%     vic-cloud        8684   [vectors]                 [vectors][+fc0]
0.03%     vic-cloud        8684   vic-cloud                 vic-cloud[+5fe30]
```

## PROCESS: vic-robot

### Top 20 functions idle

```
Overhead  Command          Tid    Shared Object             Symbol
11.41%    vic-robot        8677   libgcc                    __divdi3
9.01%     vic-robot        8677   libc                      memcpy
7.45%     vic-robot        8677   vic-robot                 calc_crc
4.60%     vic-robot        8677   libc                      memset
3.69%     vic-robot        8677   libc                      __clock_gettime
2.25%     vic-robot        8677   vic-robot                 Anki::Cozmo::spine_get_frame()
2.07%     vic-robot        8677   libc++                    std::__1::chrono::steady_clock::now()
1.87%     vic-robot        8677   vic-robot                 spine_parse_frame
1.80%     vic-robot        8677   vic-robot                 Anki::Cozmo::IMUFilter::Update()
1.35%     vic-robot        8677   vic-robot                 @plt
1.29%     vic-robot        8677   libc                      read
1.24%     vic-robot        8677   vic-robot                 Anki::Cozmo::IMUFilter::DetectPickup()
1.21%     vic-robot        8683   libc++                    std::__1::this_thread::sleep_for(std::__1::chrono::duration<long long, std::__1::ratio<1ll, 1000000000ll> > const&)
1.17%     vic-robot        8677   vic-robot                 Anki::Cozmo::HAL::Step()
1.09%     vic-robot        8677   vic-robot                 GetTimeStamp
1.00%     vic-robot        8677   vic-robot                 Anki::Cozmo::ProxSensors::UpdateCliff()
0.95%     vic-robot        8683   libc                      std::__1::chrono::steady_clock::now()
0.92%     vic-robot        8683   libc                      __clock_gettime
0.86%     vic-robot        8677   vic-robot                 Anki::Cozmo::DockingController::Update()
0.80%     vic-robot        8677   vic-robot                 Anki::Cozmo::Robot::step_MainExecution()
```

### Hierarchical idle

#### THREAD: vic-robot (87%)
```
49% vic-robot
  17% calc_crc
   5% spine_get_frame
   4% spine_parse_frame

27% libc
  38% memcpy
  19% memset

15% libc++
  87% __divdi3

5% libm
```

#### THREAD: vic-robot (12%)
```
30% vic-robot
  20% sleep
  20% ProcessIMUEvents
  16% spi_transfer
  15% imu_manage
  13% @plt

25% libc

23% libc++

16% libgcc
```

## 2/21/2018 notes richard@anki.com

### DVT2 - CPU performance

  - Large single function consumers are working with bitmap images
  - Order of expensive functions doesnt change between idle and shaken, some additions
  - vic-anim process, vic-anim thread is consistent between idle and shaken
  - pthreads more prevalent than expected

### DVT1 - CPU performance

  - Large single function consumers are working with bitmap images
  - `_raw_spin_unlock_irq` and thread functions prevalent
  - many memory allocation related functions in sub-1% band
  - Hard to find relationship between DVT1 and Webots performance

### Webots - memory performance

  - finding the jpeg encoder does ~5000 allocations!
  - 1 persistent allocation
  - 300,880 transient allocations in 3.4GB 90% of which are `intel_performance_primitives`
  - `intel_performance_primitives` appears to be running on a different thread, IPC for allocations?

# DVT2 - CPU performance

## PROCESS: vic-engine

### Top 10 functions idle

```
Overhead  Command          Tid    Shared Object             Symbol
19.26%    VisionSystem     12260  libcozmo_engine        void Anki::Embedded::ScrollingIntegralImage_u8_s32::FilterRow_innerLoop<unsigned char>(int, int, int, int, int const*, int const*, int const*, int const*, unsigned char*)
11.75%    VisionSystem     12260  libopencv_imgproc      (anonymous namespace)::CLAHE_Interpolation_Body<unsigned char, 0>::operator()(cv::Range const&) const
6.21%     VisionSystem     12261  libopencv_imgproc      (anonymous namespace)::CLAHE_Interpolation_Body<unsigned char, 0>::operator()(cv::Range const&) const
6.11%     VisionSystem     12260  libcozmo_engine        Anki::Vision::Image::BoxFilter(Anki::Vision::ImageBase<unsigned char>&, unsigned int) const
6.00%     VisionSystem     12263  libopencv_imgproc      (anonymous namespace)::CLAHE_Interpolation_Body<unsigned char, 0>::operator()(cv::Range const&) const
5.80%     VisionSystem     12262  libopencv_imgproc      (anonymous namespace)::CLAHE_Interpolation_Body<unsigned char, 0>::operator()(cv::Range const&) const
5.05%     VisionSystem     12260  libcozmo_engine        Anki::Embedded::ScrollingIntegralImage_u8_s32::ScrollDown(Anki::Embedded::Array<unsigned char> const&, int, Anki::Embedded::MemoryStack)
3.76%     VisionSystem     12260  libcozmo_engine        Anki::Embedded::ConnectedComponentsTemplate<unsigned short>::Extract1dComponents(unsigned char const*, short, short, short, Anki::Embedded::FixedLengthList<Anki::Embedded::ConnectedComponentSegment<unsigned short> >&)
3.15%     VisionSystem     12260  libcozmo_engine        Anki::Embedded::ecvcs_computeBinaryImage_numFilters3(Anki::Embedded::Array<unsigned char> const&, Anki::Embedded::FixedLengthList<Anki::Embedded::Array<unsigned char> >&, int, int, unsigned char*, bool)
1.71%     VisionSystem     12260  libcozmo_engine        Anki::Vision::ImageRGB::FillGray(Anki::Vision::Image&) const
```

### Top 10 functions shaken

```
Overhead  Command          Tid    Shared Object             Symbol
14.68%    VisionSystem     12260  libcozmo_engine        void Anki::Embedded::ScrollingIntegralImage_u8_s32::FilterRow_innerLoop<unsigned char>(int, int, int, int, int const*, int const*, int const*, int const*, unsigned char*)
6.17%     VisionSystem     12260  libcozmo_engine        OMR_F_DT_0031
5.35%     VisionSystem     12260  libopencv_imgproc      (anonymous namespace)::CLAHE_Interpolation_Body<unsigned char, 0>::operator()(cv::Range const&) const
5.24%     VisionSystem     12262  libopencv_imgproc      (anonymous namespace)::CLAHE_Interpolation_Body<unsigned char, 0>::operator()(cv::Range const&) const
4.46%     VisionSystem     13166  libopencv_imgproc      (anonymous namespace)::CLAHE_Interpolation_Body<unsigned char, 0>::operator()(cv::Range const&) const
4.27%     VisionSystem     12263  libopencv_imgproc      (anonymous namespace)::CLAHE_Interpolation_Body<unsigned char, 0>::operator()(cv::Range const&) const
3.98%     VisionSystem     12260  libcozmo_engine        Anki::Vision::Image::BoxFilter(Anki::Vision::ImageBase<unsigned char>&, unsigned int) const
3.54%     VisionSystem     12260  libcozmo_engine        Anki::Embedded::ScrollingIntegralImage_u8_s32::ScrollDown(Anki::Embedded::Array<unsigned char> const&, int, Anki::Embedded::MemoryStack)
3.01%     CozmoRunner      12205  libcozmo_engine        Anki::LineSegment::IntersectsWith(Anki::LineSegment const&) const
2.73%     VisionSystem     12260  libcozmo_engine        Anki::Embedded::ConnectedComponentsTemplate<unsigned short>::Extract1dComponents(unsigned char const*, short, short, short, Anki::Embedded::FixedLengthList<Anki::Embedded::ConnectedComponentSegment<unsigned short> >&)
```

### Hierarchical idle

#### THREAD: VisionSystem (64.1%)
```
  69.8% libcozmo_engine
    43.5% Anki::Embedded...FilterRow_innerloop
    13.8% Anki::Vision::Image::BoxFilter
    11.4% Anki::Embedded...ScrollDown
     8.5% ConnectedComponentsTemplate::Extract1Dcomponents
     7.1% ecvcs_computeBinaryImage_numFilters3

  20.5% libopencv_imgproc
    89.9% CLAHE_Interpolation_Body
     8.3% CLAHE_CalcLut_Body

  4.7%  libc
    54.2% memset
    22.4% memcpy
     7.9% pthread_getspecific
```

#### THREAD: CozmoRunner (12.9%)
```
  60.3% libcozmo_engine
    19.8% Anki::LineSegment::IntersectsWith
    10.7% Anki::QuadTreeNode::GetOverlapType
     7.6% Anki::PoseBase::DevAssert_IsValidParentPointer

  24.4% libc
    24.4% pthread_mutex_unlock
    18.4% pthread_mutex_lock
     9.8% pthread_getspecific
     7.4% @plt

   8.6% libc++
    38.2% __release_shared
```

#### THREAD: VisionSystem (7.8%)
```
  ... CLAHE_Interpolation_Body
```

#### THREAD: VisionSystem (7.6%)
```
  ... CLAHE_Interpolation_Body
```

#### THREAD: VisionSystem (7.4%)
```
  ... CLAHE_Interpolation_Body
```

### Hierarchical shaken

#### THREAD: VisionSystem (58.0% < 64.1%)
```
  79.8% libcozmo_engine (> 69.8%)
    32.1% Anki::Embedded...FilterRow_innerloop (< 43.5%)
    13.5% OMR_F_DT_0031 ()
     8.7% Anki::Vision::Image::BoxFilter (< 13.8%)
     7.7% Anki::Embedded...ScrollDown (< 11.4%)
     6.0% ConnectedComponentsTemplate::Extract1Dcomponents (8.5%)
     5.6% OMR_F_DT_0048 ()
     4.8% ecvcs_computeBinaryImage_numFilters3 (7.1%)

  13.8% libopencv_imgproc (< 20.5%)
    67.3% CLAHE_Interpolation_Body (< 89.9%)
     7.1% cv::ColumnSum ()
     5.9% cv::resizeNNInvoker ()
     4.9% CLAHE_CalcLut_Body (<8.3%)

  3.5% libc (<4.7%)
    53.0% memset (<54.2%)
    27.5% memcpy (>22.4%)
     5.1% pthread_getspecific (<7.9%)
```

#### THREAD: CozmoRunner (22.7% > 12.9%)
```
  61.3% libcozmo_engine (~= 60.3%)
    19.8% Anki::LineSegment::IntersectsWith
    10.7% Anki::QuadTreeNode::GetOverlapType
     7.6% Anki::PoseBase::DevAssert_IsValidParentPointer

  21.4% libc (~= 24.4%)
    18.7% pthread_mutex_unlock (< 24.4%)
    13.3% pthread_mutex_lock (< 18.4%)
     8.7% pthread_getspecific (~= 9.8%)
     6.2% @plt (~= 7.4%)

   9.8% libc++ (~= 8.6%)
    43.2% __release_shared (> 38.2%)
```

#### THREAD: VisionSystem (6.5% < 7.8%)
```
  ... CLAHE_Interpolation_Body
```

#### THREAD: VisionSystem (5.6% < 7.6%)
```
  ... CLAHE_Interpolation_Body
```

#### THREAD: VisionSystem (5.3% < 7.4%)
```
  ... CLAHE_Interpolation_Body
```

## PROCESS: vic-anim

### Top 10 functions idle

```
Overhead  Command          Tid    Shared Object             Symbol
11.74%    MicDataProc      12190  vic-anim                  .i_loop_no_load
8.74%     MicDataProc      12190  vic-anim                  u139y
6.10%     vic-anim         12183  vic-anim                  Anki::Cozmo::ProceduralFaceDrawer::DrawEye(Anki::Cozmo::ProceduralFace const&, Anki::Cozmo::ProceduralFace::WhichEye, Anki::Util::RandomGenerator const&, Anki::Vision::ImageRGB&, Anki::Rectangle<float>&)
4.87%     MicDataProc      12190  vic-anim                  VGenComplexTone_i16_ansi
4.20%     vic-anim         12183  vic-anim                  Anki::Vision::ImageRGB::ConvertHSV2RGB565(Anki::Vision::ImageRGB565&)
3.55%     vic-anim         12183  libopencv_imgproc      void cv::remapNearest<unsigned char>(cv::Mat const&, cv::Mat&, cv::Mat const&, int, cv::Scalar_<double> const&)
3.50%     MicDataProc      12190  libc                   memcpy
2.71%     MicDataProc      12190  vic-anim                  ConvertFloatToInt16
2.69%     MicDataProc      12190  vic-anim                  radf4_ps
2.25%     vic-anim         12183  libm                   floorf
```

### Top 10 functions shaken

```
Overhead  Command          Tid    Shared Object             Symbol
47.78%    MicDataProc      12190  vic-anim                  u139y
4.85%     MicDataProc      12190  vic-anim                  .i_loop_no_load
3.35%     vic-anim         12183  vic-anim                  Anki::Cozmo::ProceduralFaceDrawer::DrawEye(Anki::Cozmo::ProceduralFace const&, Anki::Cozmo::ProceduralFace::WhichEye, Anki::Util::RandomGenerator const&, Anki::Vision::ImageRGB&, Anki::Rectangle<float>&)
2.46%     MicDataProc      12190  vic-anim                  .loop_8
2.38%     vic-anim         12183  vic-anim                  Anki::Vision::ImageRGB::ConvertHSV2RGB565(Anki::Vision::ImageRGB565&)
2.23%     MicDataProc      12190  vic-anim                  a562j
2.03%     vic-anim         12183  libopencv_imgproc      void cv::remapNearest<unsigned char>(cv::Mat const&, cv::Mat&, cv::Mat const&, int, cv::Scalar_<double> const&)
1.99%     MicDataProc      12190  vic-anim                  VGenComplexTone_i16_ansi
1.65%     MicDataProc      12190  vic-anim                  b383i
1.54%     MicDataProc      12190  libc                   memcpy
```

### Hierarchical idle

#### THREAD: MicDataProc (64%)
```
90% vic-anim
  20.3% i_loop_no_load
  15.1% u139y
   8.4% VGenComplexTone_i16_ansi
   4.7% ConvertFloatToInt16

7.5% libc
  78.0% memcpy
   3.9% memset
```

#### THREAD: vic-anim (34.7%)
```
38.4% vic-anim
  47.3% DrawEye
  32.6% ConvertHSV2RGB565
   7.9% ApplyScanlines
   4.0% @plt

28.5% libopencv_imgproc
  35.9% cv::remapNearest
  13.6% cv::ColumnSum
  11.4% cv::RowSum
  10.7% cv::LineAA

13.4% libm
  48.0% floorf
  42.1% roundf
   8.0% @plt

12.4% libc
  48.6% memcpy
   7.2% pthread_mutex_lock
   6.8% pthread_mutex_unlock
```

### Hierarchical shaken

#### THREAD: MicDataProc (79.9% > 64%)
```
96.7% vic-anim (> 90%)
  62.2% u139y (> 15.1%)
   6.3% i_loop_no_load (< 20.3%)
   4.4% <other>
   3.2% loop_8
   2.6% VGenComplexTone_i16_ansi (< 8.4%)
   1.4% ConvertFloatToInt16 (4.7%)

2.6% libc (< 7.5%)
  81.9% memcpy (> 78.0%)
   4.1% memset (> 3.9%)
```

#### THREAD: vic-anim (19.9% < 34.7%)
```
38.4% vic-anim (== 38.4%)
  48.3% DrawEye (~= 47.3%)
  34.4% ConvertHSV2RGB565 (~= 32.6%)
   8.3% ApplyScanlines (~= 7.9%)
   3.9% @plt (== 4.0%)

28.4% libopencv_imgproc (== 28.5%)
  38.1% cv::remapNearest (> 35.9%)
  12.4% cv::ColumnSum (> 13.6%)
  10.9% cv::RowSum (< 11.4%)
  11.5% cv::LineAA (< 10.7%)

14.1% libm (~= 13.4%)
  49.6% floorf (~= 48.0%)
  41.4% roundf (~= 42.1%)
   7.7% @plt (~= 8.0%)

11.9% libc (~= 12.4%)
  55.8% memcpy (> 48.6%)
   7.9% pthread_mutex_lock (> 7.2%)
   7.1% pthread_mutex_unlock (> 6.8%)
   7.1% pthread_getspecific ()
```

# DVT1 - CPU performance

## PROCESS: cozmoengined

#### THREAD: VisionSystem
```
  23%    Anki::Embedded::ScrollingIntegralImage_u8_s32::FilterRow_innerLoop
  11%    CLAHE_Interpolation_Body
   7%    Anki::Embedded::ScrollingIntegralImage_u8_s32::ScrollDown
   6%    Anki::Embedded::ExtractLineFitsPeaks
   5%    Anki::Embedded::ConnectedComponentsTemplate
   3%    Anki::Embedded::ecvcs_computeBinaryImage_numFilters3
   2%    Anki::Vision::ImageRGB::FillGray
   1%    CLAHE_CalcLut_Body
   1%    Anki::Embedded::ScrollingIntegralImage_u8_s32::PadImageRow
```

#### THREAD: CozmoRunner
```
   3%    Anki::Cozmo::UiMessageHandler::Update()
   3%    je_arena_malloc_hard
   3%    arena_dalloc_bin_locked_impl
   2%    je_malloc
   2%    arena_run_reg_alloc
   1%    unlock 
   1%    Anki::PoseBase<Anki::Pose3d, Anki::Transform3d>::PoseTreeNode::Dev_AssertIsValidParentPointer
```

#### THREAD: VisionSystem
```
  61%    CLAHE_Interpolation_Body
  20%    _raw_spin_unlock_irq
   7%    CLAHE_CalcLut_Body
   2%    cv::KMeansDistanceComputer::operator()
```

#### THREAD: VisionSystem
```
  62%    CLAHE_Interpolation_Body
  17%    _raw_spin_unlock_irq
   6%    CLAHE_CalcLut_Body
   2%    cv::KMeansDistanceComputer::operator()
```

#### THREAD: VisionSystem
```
  63%    CLAHE_Interpolation_Body
  15%    _raw_spin_unlock_irq
   7%    CLAHE_CalcLut_Body
   2%    cv::KMeansDistanceComputer::operator()
```

#### THREAD: vic-engine
```
  48%    _raw_spin_unlock_irqrestore
   7%    usleep
   2.5%  IsRunning
```

#### THREAD: mm_cam_stream
```
  75%    mm_app_snapshot_notify_cb_raw
  13%    v7_dma_inv_range
```

#### THREAD: FaceRecognizer
```
  40%    _raw_spin_unlock_irqrestore
   4%    FaceRecognizer::Run
   7%    sleep
   2%    thread_mutex_lock
   1%    thread_mutex_unlock
```

#### THREAD: BehaviorServer
```
  36%    _raw_spin_unlock_irqrestore
  11%    mutex
```

#### THREAD: civetweb-master
```
  82%    poll
```

#### THREAD: camera?
```
  51%    v7_dma_flush_range
```


## PROCESS: vic-anim

#### THREAD: MicDataProc
```
  26%    _raw_spin_unlock_irqrestore
   8%    Anki::Cozmo::MicData::MicDataProcessor::ProcessLoop
   4%    sleep
```

#### THREAD: vic-anim
```
  18%    cv::HSV2RGB...
  11%    cv::remapNearest
   9%    Anki::Cozmo::ProceduralFaceDrawer::DrawEye
   6%    cv_vrndq_u32_f32
   6%    memcpy
   3%    cv::LineAA
   3%    cv::WarpAffineInvoker::operator()
   2%    floor
   2%    round
   2%    cv::RGB2RGB5x5::operator()
   1%    cv::fastMalloc
```

#### THREAD: DrawFaceLoop
```
  75%    write
  17%    sleep
   2%    Anki::Cozmo::FaceDisplay::DrawFaceLoop()
```

#### THREAD: civetweb-master
```
  86%    poll
   6%    master_thread
```

#### THREAD: wwise
```
  36%    sleep
```

#### THREAD: wwise
```
  32%    sleep
```

# Webots - CPU performance

## PROCESS: cozmoengined

### THREAD: main thread
```
  99%    CozmoAPI::Update
    87%    Robot::Update
      70%    VisionComponent::Update
        47%    VisionComponent::Update::CaptureImage
        22%    VisionComponent::CompressAndSendImage
    11%    RobotManager::UpdateRobotConnection
```

#### THREAD: VISIONPROCESSORTHREAD
```
  86%    UpdateVisionSystem
  ...
  76%    Anki::Embedded::DetectFiducialMarkers
  54%    Anki::Embedded::ExtraComponentsViaCharacterIsticScale

  ultimately ucvcs_ functions
```

#### THREAD: TASKEXECUTORTHREAD
```
  57%    Anki::Util::RollingFileLogger
    53%    ostream.flush
```

## PROCESS: vic-anim

#### THREAD: MAIN THREAD
```
  51%    AnimationStreamer::Update
    47%    AnimationStreamer::StreamLayers
    45%    AnimationStreamer::BufferFaceToSend
  31%    AnimProcessMessages::Update
    13%    GetNextPacketFromRobot => UDP
    10%    ProcessMessages => UDP
     4%    MicDataProcessor
```

#### THREAD: MIC PROCESSOR
```
  38%    MicDataProcessor::ProcessResampledAudio
  35%    MicDataProcessor::ResampleAudioChunk
  23%    SpeechRecognizerTHF
```

#### THREAD: WWISE

#### THREAD: FACE DISPLAY
```
  65%   sleeping
```

# Webots - memory performance

#### Cozmo process
```
  69%,         422MB, 1910594   Cozmo::Robot::UpdateAllRobots
    68%,       420MB, 1885073     Cozmo::Robot::Update
    48%,       298MB,  160492       Cozmo::VisionComponent::Update
      26%,     163MB,   63840         Cozmo::VisionComponent::CompressAndSendImage
        19.3%, 117MB,   23940           cv::JpegEncoder
         9.0%, 274KB,    4788           cv::findEncoder
    16%,       100MB, 1478710       Cozmo::VisionComponent::UpdateAllResults
     6%,        40MB,  528276         CreateObjectsFromMarkers
     5%,        30MB,  489172         AddAndUpdateObjects
     2.5%,      15MB,  239400         UpdatePoseOfStackedObjcets
  13%,          99MB,  1508447  Cozmo::Robot::UpdateRobotConnection
```

#### Anim process
```
  90%,         3.2GB, 196785    ippMalloc
   4%,         156MB,  61374    Cozmo::AnimEngine::Update
```

# Performance costs of features in face rendering

Sampled over 60 seconds on DVT2, Victor was shaken.

Key             |Options
----------------|-------
COZMO RENDERING | none of the new Victor features
VICTOR RENDERING| all of the Victor features enabled
X-glowfilt      | all Victor features enabled except kProcFace_ApplyGlowFilter
X-aliasing      | all Victor features enabled except kProcFace_AntiAliasingSize
X-outerglow     | all Victor features enabled except kProcFace_RenderInnerOuterGlow
X-noise         | all Victor features enabled except kProcFace_UseNoise

## Notes

vic-anim process is dominated by the MicDataThread, 47%.
Cozmo rendering costs 2.35% per eye
Victor rendering costs 4.87% per eye
OpenCV filtering costs ~2%
Worst case cost ~22% (4.87% + 4.87% + 3.75% + 2.97% + 1.83% + 1.62% + 1.09% + 0.87% + 0.87%)

## Suggestions

ApplyScanlines NEONized
Test rendering quality with lines AA or not
Triangle rasterization NEONized

```
47.07%    VICTOR RENDERING X-glowfilt   MicDataProc      u139y
46.90%    VICTOR RENDERING X-aliasing   MicDataProc      u139y
44.54%    VICTOR RENDERING X-outerglow  MicDataProc      u139y
40.33%    COZMO RENDERING               MicDataProc      u139y
35.11%    VICTOR RENDERING X-noise      MicDataProc      u139y
34.35%    VICTOR RENDERING              MicDataProc      u139y

6.17%     COZMO RENDERING               MicDataProc      .i_loop_no_load
5.88%     VICTOR RENDERING              MicDataProc      .i_loop_no_load
5.76%     VICTOR RENDERING X-noise      MicDataProc      .i_loop_no_load
5.48%     VICTOR RENDERING X-outerglow  MicDataProc      .i_loop_no_load
5.28%     VICTOR RENDERING X-glowfilt   MicDataProc      .i_loop_no_load
4.77%     VICTOR RENDERING X-aliasing   MicDataProc      .i_loop_no_load

4.87%     VICTOR RENDERING X-noise      vic-anim         Anki::Cozmo::ProceduralFaceDrawer::DrawEye(Anki::Cozmo::ProceduralFace const&, Anki::Cozmo::ProceduralFace::WhichEye, Anki::Util::RandomGenerator const&, Anki::Vision::ImageRGB&, Anki::Rectangle<float>&)
4.81%     VICTOR RENDERING              vic-anim         Anki::Cozmo::ProceduralFaceDrawer::DrawEye(Anki::Cozmo::ProceduralFace const&, Anki::Cozmo::ProceduralFace::WhichEye, Anki::Util::RandomGenerator const&, Anki::Vision::ImageRGB&, Anki::Rectangle<float>&)
2.72%     VICTOR RENDERING X-aliasing   vic-anim         Anki::Cozmo::ProceduralFaceDrawer::DrawEye(Anki::Cozmo::ProceduralFace const&, Anki::Cozmo::ProceduralFace::WhichEye, Anki::Util::RandomGenerator const&, Anki::Vision::ImageRGB&, Anki::Rectangle<float>&)
2.47%     VICTOR RENDERING X-outerglow  vic-anim         Anki::Cozmo::ProceduralFaceDrawer::DrawEye(Anki::Cozmo::ProceduralFace const&, Anki::Cozmo::ProceduralFace::WhichEye, Anki::Util::RandomGenerator const&, Anki::Vision::ImageRGB&, Anki::Rectangle<float>&)
2.39%     VICTOR RENDERING X-glowfilt   vic-anim         Anki::Cozmo::ProceduralFaceDrawer::DrawEye(Anki::Cozmo::ProceduralFace const&, Anki::Cozmo::ProceduralFace::WhichEye, Anki::Util::RandomGenerator const&, Anki::Vision::ImageRGB&, Anki::Rectangle<float>&)
2.35%     COZMO RENDERING               vic-anim         Anki::Cozmo::ProceduralFaceDrawer::DrawEye(Anki::Cozmo::ProceduralFace const&, Anki::Cozmo::ProceduralFace::WhichEye, Anki::Util::RandomGenerator const&, Anki::Vision::ImageRGB&, Anki::Rectangle<float>&)

3.75%     COZMO RENDERING               vic-anim         Anki::Vision::ImageRGB::ConvertHSV2RGB565(Anki::Vision::ImageRGB565&)
3.48%     VICTOR RENDERING X-noise      vic-anim         Anki::Vision::ImageRGB::ConvertHSV2RGB565(Anki::Vision::ImageRGB565&)
3.44%     VICTOR RENDERING              vic-anim         Anki::Vision::ImageRGB::ConvertHSV2RGB565(Anki::Vision::ImageRGB565&)
2.84%     VICTOR RENDERING X-outerglow  vic-anim         Anki::Vision::ImageRGB::ConvertHSV2RGB565(Anki::Vision::ImageRGB565&)
2.47%     VICTOR RENDERING X-glowfilt   vic-anim         Anki::Vision::ImageRGB::ConvertHSV2RGB565(Anki::Vision::ImageRGB565&)
2.37%     VICTOR RENDERING X-aliasing   vic-anim         Anki::Vision::ImageRGB::ConvertHSV2RGB565(Anki::Vision::ImageRGB565&)

2.97%     COZMO RENDERING               vic-anim         void cv::remapNearest<unsigned char>(cv::Mat const&, cv::Mat&, cv::Mat const&, int, cv::Scalar_<double> const&)
2.78%     VICTOR RENDERING X-noise      vic-anim         void cv::remapNearest<unsigned char>(cv::Mat const&, cv::Mat&, cv::Mat const&, int, cv::Scalar_<double> const&)
2.69%     VICTOR RENDERING              vic-anim         void cv::remapNearest<unsigned char>(cv::Mat const&, cv::Mat&, cv::Mat const&, int, cv::Scalar_<double> const&)
2.09%     VICTOR RENDERING X-glowfilt   vic-anim         void cv::remapNearest<unsigned char>(cv::Mat const&, cv::Mat&, cv::Mat const&, int, cv::Scalar_<double> const&)
2.06%     VICTOR RENDERING X-outerglow  vic-anim         void cv::remapNearest<unsigned char>(cv::Mat const&, cv::Mat&, cv::Mat const&, int, cv::Scalar_<double> const&)
2.04%     VICTOR RENDERING X-aliasing   vic-anim         void cv::remapNearest<unsigned char>(cv::Mat const&, cv::Mat&, cv::Mat const&, int, cv::Scalar_<double> const&)

1.83%     VICTOR RENDERING X-noise      vic-anim         floorf
1.76%     VICTOR RENDERING              vic-anim         floorf
1.04%     VICTOR RENDERING X-outerglow  vic-anim         floorf
0.90%     COZMO RENDERING               vic-anim         floorf
0.85%     VICTOR RENDERING X-aliasing   vic-anim         floorf
0.74%     VICTOR RENDERING X-glowfilt   vic-anim         floorf

1.62%     VICTOR RENDERING              vic-anim         roundf
1.60%     VICTOR RENDERING X-noise      vic-anim         roundf
0.89%     VICTOR RENDERING X-outerglow  vic-anim         roundf
0.73%     VICTOR RENDERING X-aliasing   vic-anim         roundf
0.71%     COZMO RENDERING               vic-anim         roundf
0.62%     VICTOR RENDERING X-glowfilt   vic-anim         roundf

1.09%     VICTOR RENDERING X-noise      vic-anim         cv::ColumnSum<unsigned short, unsigned char>::operator()(unsigned char const**, unsigned char*, int, int, int)
1.05%     VICTOR RENDERING              vic-anim         cv::ColumnSum<unsigned short, unsigned char>::operator()(unsigned char const**, unsigned char*, int, int, int)
0.76%     VICTOR RENDERING X-outerglow  vic-anim         cv::ColumnSum<unsigned short, unsigned char>::operator()(unsigned char const**, unsigned char*, int, int, int)
0.38%     VICTOR RENDERING X-outerglow  vic-anim         cv::ColumnSum<int, unsigned char>::operator()(unsigned char const**, unsigned char*, int, int, int)
0.37%     VICTOR RENDERING X-aliasing   vic-anim         cv::ColumnSum<unsigned short, unsigned char>::operator()(unsigned char const**, unsigned char*, int, int, int)
0.36%     VICTOR RENDERING X-glowfilt   vic-anim         cv::ColumnSum<unsigned short, unsigned char>::operator()(unsigned char const**, unsigned char*, int, int, int)

0.87%     COZMO RENDERING               vic-anim         Anki::Cozmo::ProceduralFaceDrawer::ApplyScanlines(Anki::Vision::ImageRGB&, float)
0.80%     VICTOR RENDERING X-noise      vic-anim         Anki::Cozmo::ProceduralFaceDrawer::ApplyScanlines(Anki::Vision::ImageRGB&, float)
0.80%     VICTOR RENDERING              vic-anim         Anki::Cozmo::ProceduralFaceDrawer::ApplyScanlines(Anki::Vision::ImageRGB&, float)
0.60%     VICTOR RENDERING X-outerglow  vic-anim         Anki::Cozmo::ProceduralFaceDrawer::ApplyScanlines(Anki::Vision::ImageRGB&, float)
0.60%     VICTOR RENDERING X-glowfilt   vic-anim         Anki::Cozmo::ProceduralFaceDrawer::ApplyScanlines(Anki::Vision::ImageRGB&, float)
0.60%     VICTOR RENDERING X-aliasing   vic-anim         Anki::Cozmo::ProceduralFaceDrawer::ApplyScanlines(Anki::Vision::ImageRGB&, float)

0.87%     COZMO RENDERING               vic-anim         cv::LineAA(cv::Mat&, cv::Point_<long long>, cv::Point_<long long>, void const*)
0.80%     VICTOR RENDERING              vic-anim         cv::LineAA(cv::Mat&, cv::Point_<long long>, cv::Point_<long long>, void const*)
0.78%     VICTOR RENDERING X-noise      vic-anim         cv::LineAA(cv::Mat&, cv::Point_<long long>, cv::Point_<long long>, void const*)
0.63%     VICTOR RENDERING X-outerglow  vic-anim         cv::LineAA(cv::Mat&, cv::Point_<long long>, cv::Point_<long long>, void const*)
0.62%     VICTOR RENDERING X-glowfilt   vic-anim         cv::LineAA(cv::Mat&, cv::Point_<long long>, cv::Point_<long long>, void const*)
0.61%     VICTOR RENDERING X-aliasing   vic-anim         cv::LineAA(cv::Mat&, cv::Point_<long long>, cv::Point_<long long>, void const*)

0.73%     COZMO RENDERING               vic-anim         cv::WarpAffineInvoker::operator()(cv::Range const&) const
0.68%     VICTOR RENDERING              vic-anim         cv::WarpAffineInvoker::operator()(cv::Range const&) const
0.65%     VICTOR RENDERING X-noise      vic-anim         cv::WarpAffineInvoker::operator()(cv::Range const&) const
0.49%     VICTOR RENDERING X-glowfilt   vic-anim         cv::WarpAffineInvoker::operator()(cv::Range const&) const
0.49%     VICTOR RENDERING X-aliasing   vic-anim         cv::WarpAffineInvoker::operator()(cv::Range const&) const
0.48%     VICTOR RENDERING X-outerglow  vic-anim         cv::WarpAffineInvoker::operator()(cv::Range const&) const

0.97%     VICTOR RENDERING X-noise      vic-anim         memcpy
0.95%     VICTOR RENDERING              vic-anim         memcpy
0.89%     COZMO RENDERING               vic-anim         memcpy
0.74%     VICTOR RENDERING X-outerglow  vic-anim         memcpy
0.67%     VICTOR RENDERING X-aliasing   vic-anim         memcpy
0.65%     VICTOR RENDERING X-glowfilt   vic-anim         memcpy

0.32%     VICTOR RENDERING              vic-anim         cv::FilterEngine::proceed(unsigned char const*, int, int, unsigned char*, int)
0.27%     VICTOR RENDERING X-outerglow  vic-anim         cv::FilterEngine::proceed(unsigned char const*, int, int, unsigned char*, int)

0.90%     VICTOR RENDERING X-noise      vic-anim         cv::RowSum<unsigned char, unsigned short>::operator()(unsigned char const*, unsigned char*, int, int)
0.90%     VICTOR RENDERING              vic-anim         cv::RowSum<unsigned char, unsigned short>::operator()(unsigned char const*, unsigned char*, int, int)
0.65%     VICTOR RENDERING X-outerglow  vic-anim         cv::RowSum<unsigned char, unsigned short>::operator()(unsigned char const*, unsigned char*, int, int)
0.38%     VICTOR RENDERING X-aliasing   vic-anim         cv::RowSum<unsigned char, unsigned short>::operator()(unsigned char const*, unsigned char*, int, int)
0.25%     VICTOR RENDERING X-glowfilt   vic-anim         cv::RowSum<unsigned char, unsigned short>::operator()(unsigned char const*, unsigned char*, int, int)
```
