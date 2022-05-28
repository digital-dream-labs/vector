/**
 * File: camera_client.h
 *
 * Author: Brian Chapados
 * Created: 01/24/2018
 *
 * Description:
 *               API for remote IPC connection to anki camera system daemon
 *
 * Copyright: Anki, Inc. 2018
 *
 **/

#ifndef __platform_hal_camera_client_h__
#define __platform_hal_camera_client_h__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum {
  ANKI_CAMERA_STATUS_OFFLINE,
  ANKI_CAMERA_STATUS_IDLE,
  ANKI_CAMERA_STATUS_STARTING,
  ANKI_CAMERA_STATUS_RUNNING,
} anki_camera_status_t;

// BEGIN: shared types
// These types are shared with the server component in the OS.
// Eventually these structs will be available via a header in our
// custom toolchain, or we will move the camera system back into the
// engine instead of using a separate process.

//
// IPC Message Protocol
//
typedef enum {
  ANKI_CAMERA_MSG_C2S_HEARTBEAT,
  ANKI_CAMERA_MSG_C2S_CLIENT_REGISTER,
  ANKI_CAMERA_MSG_C2S_CLIENT_UNREGISTER,
  ANKI_CAMERA_MSG_C2S_START,
  ANKI_CAMERA_MSG_C2S_STOP,
  ANKI_CAMERA_MSG_C2S_PARAMS,
  ANKI_CAMERA_MSG_S2C_STATUS,
  ANKI_CAMERA_MSG_S2C_BUFFER,
  ANKI_CAMERA_MSG_S2C_HEARTBEAT,
} anki_camera_msg_id_t;

#define ANKI_CAMERA_MSG_PAYLOAD_LEN 128

struct anki_camera_msg {
  anki_camera_msg_id_t msg_id;
  uint32_t version;
  uint32_t client_id;
  int fd;
  uint8_t payload[ANKI_CAMERA_MSG_PAYLOAD_LEN];
};

typedef struct {
  uint64_t timestamp;
  uint32_t frame_id;
  uint32_t width;
  uint32_t height;
  uint32_t bytes_per_row;
  uint8_t  bits_per_pixel;
  uint8_t  format;
  uint8_t  _reserved[2];
  uint32_t _pad_to_64[8];
  uint8_t  data[0];
} anki_camera_frame_t;

typedef struct {
  uint16_t exposure_ms;
  float gain;
} anki_camera_exposure_t;

typedef struct {
  float r_gain;
  float g_gain;
  float b_gain;
} anki_camera_awb_t;

typedef enum {
  ANKI_CAMERA_MSG_C2S_PARAMS_ID_EXP,
  ANKI_CAMERA_MSG_C2S_PARAMS_ID_AWB,
  ANKI_CAMERA_MSG_C2S_PARAMS_ID_FORMAT,
  ANKI_CAMERA_MSG_C2S_PARAMS_ID_SNAPSHOT,
} anki_camera_params_id_t;

typedef struct {
  anki_camera_params_id_t id;
  uint8_t data[sizeof(((struct anki_camera_msg*)0)->payload) - sizeof(anki_camera_params_id_t)];
} anki_camera_msg_params_payload_t;

typedef enum {
  ANKI_CAM_FORMAT_BAYER_MIPI_BGGR10,
  ANKI_CAM_FORMAT_RAW = ANKI_CAM_FORMAT_BAYER_MIPI_BGGR10,
  ANKI_CAM_FORMAT_RGB888,
  ANKI_CAM_FORMAT_YUV,
} anki_camera_pixel_format_t;
  
// END: shared types


struct anki_camera_handle {
  int client_handle;
  uint32_t current_frame_id;
  uint32_t last_frame_slot;
};

// Initializes the camera & starts thread for communicating with daemon
int camera_init(struct anki_camera_handle** camera);

// Starts capturing frames
// Captured frames are buffered internally and can be accessed by calling
// `camera_frame_acquire`.
int camera_start(struct anki_camera_handle* camera);

// Stops capturing frames
// Completely stops camera stream and tears down buffers
// Note: Bugs in Qualcomm code mean this function does not always work
// sometimes the camera service completely hangs...
int camera_stop(struct anki_camera_handle* camera);

// Pauses the camera stream leaving everything in a valid state
// Note: The first image captured after unpausing will be invalid
// This is because we don't know where in the image capture cycle the
// camera is so we are likely stopping it half way through capturing an image.
// When it unpauses it will finish capturing that image but the data will be invalid
void camera_pause(struct anki_camera_handle* camera, int pause);
  
// De-initializes camera, makes it available to rest of system
// This is asynchronous, check return value of camera_destroy
// to know when the camera has actually been released
int camera_release(struct anki_camera_handle* camera);

// Attempts to destroy a previously released camera
// Returns 1 if camera has been successfully destroyed, 0 otherwise
int camera_destroy(struct anki_camera_handle* camera);
  
// Acquire (lock) the most recent available frame for reading
int camera_frame_acquire(struct anki_camera_handle* camera,
                         uint64_t frame_timestamp,
                         anki_camera_frame_t** out_frame);

// Release (unlock) frame to camera system
int camera_frame_release(struct anki_camera_handle* camera, uint32_t frame_id);

int camera_set_exposure(struct anki_camera_handle* camera, uint16_t exposure_ms, float gain);

int camera_set_awb(struct anki_camera_handle* camera, float r_gain, float g_gain, float b_gain);

int camera_set_capture_format(struct anki_camera_handle* camera, anki_camera_pixel_format_t format);


int camera_set_capture_snapshot(struct anki_camera_handle* camera,
                                uint8_t start);
 
// Get current status of camera system
anki_camera_status_t camera_status(struct anki_camera_handle* camera);

#ifdef __cplusplus
}
#endif

#endif // __platform_hal_camera_client_h__
