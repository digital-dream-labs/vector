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
// To properly include `pthread_setname_np` function we need to define GNU_SOURCE
#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdatomic.h>
#include <pthread.h>

// linux kernel header provided for ION memory access
#include "linux/ion.h"

#include "camera_client.h"
#include "log.h"
#include "platform/gpio/gpio.h"

static char *cli_socket_path = "/dev/socket/vic-engine-cam_client0";
static char *srv_socket_path = "/var/run/mm-anki-camera/camera-server";

static const uint64_t HEARTBEAT_INTERVAL_NS = 200000000;
static const uint64_t HEARTBEAT_INTERVAL_US = 200000;

#define NSEC_PER_SEC ((uint64_t)1000000000)
#define NSEC_PER_MSEC ((uint64_t)1000000)
#define NSEC_PER_USEC (1000)

// BEGIN: shared types
// These types are shared with the server component in the OS.
// Eventually these structs will be available via a header in our
// custom toolchain, or we will move the camera system back into the
// engine instead of using a separate process.

#define ANKI_CAMERA_MAX_PACKETS 12
#define ANKI_CAMERA_MAX_FRAME_COUNT 6

//
// ION memory management info
//
struct camera_capture_mem_info {
  int camera_capture_fd;
  int ion_fd;
  ion_user_handle_t ion_handle;
  uint32_t size;
  uint8_t *data;
};

//
// Internal Layout of shared camera capture memory
//
typedef struct {
  _Atomic uint32_t write_idx;
  _Atomic uint32_t frame_locks[ANKI_CAMERA_MAX_FRAME_COUNT];
} anki_camera_buf_lock_t;

typedef struct {
  uint8_t magic[4];
  anki_camera_buf_lock_t locks;
  uint32_t frame_count;
  uint32_t frame_size;
  uint32_t frame_offsets[ANKI_CAMERA_MAX_FRAME_COUNT];
  uint8_t data[0];
} anki_camera_buf_header_t;

//
// IPC Client State
//
struct client_ctx {
  pthread_t ipc_thread;
  pthread_mutex_t ipc_mutex;
  int waiting_for_delete;

  int fd;
  int is_running;
  int request_close;
  int request_start;
  anki_camera_status_t status;

  struct camera_capture_mem_info camera_buf;
  uint64_t locked_slots[ANKI_CAMERA_MAX_FRAME_COUNT];

  uint32_t rx_cursor;
  struct anki_camera_msg rx_packets[ANKI_CAMERA_MAX_PACKETS];

  uint32_t tx_cursor;
  struct anki_camera_msg tx_packets[ANKI_CAMERA_MAX_PACKETS];
};

//
// Internal extended version of public struct
//
struct anki_camera_handle_private {
  int client_handle;
  uint32_t current_frame_id;
  uint32_t last_frame_slot;

  // private
  struct client_ctx camera_client;
};

static struct anki_camera_handle_private s_camera_handle;

#define PWDN_PIN 94
static GPIO s_pwdn_gpio; // gpio for camera power down pin

static ssize_t read_fd(int fd, void *ptr, size_t nbytes, int *recvfd)
{
  struct msghdr msg;
  struct iovec iov[1];
  ssize_t n;

  *recvfd = -1; /* default: descriptor was not passed */

  union {
    struct cmsghdr cm;
    char control[CMSG_SPACE(sizeof(int))];
  } control_un;
  struct cmsghdr *cmptr;

  msg.msg_control = control_un.control;
  msg.msg_controllen = sizeof(control_un.control);

  msg.msg_name = NULL;
  msg.msg_namelen = 0;

  iov[0].iov_base = ptr;
  iov[0].iov_len = nbytes;
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;

  n = recvmsg(fd, &msg, 0);
  if (n <= 0) {
    return (n);
  }

  cmptr = CMSG_FIRSTHDR(&msg);
  if ((cmptr != NULL) &&
      (cmptr->cmsg_len == CMSG_LEN(sizeof(int)))) {
    if (cmptr->cmsg_level != SOL_SOCKET) {
      loge("%s: control level != SOL_SOCKET: %s", __func__, strerror(errno));
      return -1;
    }
    if (cmptr->cmsg_type != SCM_RIGHTS) {
      loge("%s: control type != SCM_RIGHTS: %s", __func__, strerror(errno));
      return -1;
    }
    *recvfd = *((int *)CMSG_DATA(cmptr));
  }

  return (n);
}

static int configure_socket(int socket)
{
  const int enable = 1;
  const int status = setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

  return status;
}

static int socket_connect(int *out_fd)
{
  struct sockaddr_un caddr;
  struct sockaddr_un saddr;
  int fd = -1;
  int rc = 0;
  *out_fd = -1;

  fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0);
  if (fd == -1) {
    loge("%s: socket error: %s", __func__, strerror(errno));
    return -1;
  }

  // configure (reuse)
  rc = configure_socket(fd);
  if (rc == -1) {
    loge("%s: socket configuration error: %s", __func__, strerror(errno));
    return -1;
  }

  // bind client socket
  memset(&caddr, 0, sizeof(caddr));
  caddr.sun_family = AF_UNIX;
  strncpy(caddr.sun_path, cli_socket_path, sizeof(caddr.sun_path) - 1);
  unlink(cli_socket_path);

  rc = bind(fd, (struct sockaddr *)&caddr, sizeof(caddr));
  if (rc == -1) {
    loge("%s: bind error: %s", __func__, strerror(errno));
    return -1;
  }

  // connect to server socket
  memset(&saddr, 0, sizeof(saddr));
  saddr.sun_family = AF_UNIX;
  strncpy(saddr.sun_path, srv_socket_path, sizeof(saddr.sun_path) - 1);

  rc = connect(fd, (struct sockaddr *)&saddr, sizeof(saddr));
  if (rc == -1) {
    loge("%s: connect error: %s", __func__, strerror(errno));
    return -1;
  }

  *out_fd = fd;
  return rc;
}

static int send_message(struct client_ctx *ctx, struct anki_camera_msg *msg)
{
  ssize_t bytes_sent = write(ctx->fd, msg, sizeof(*msg));
  if (bytes_sent != sizeof(*msg)) {
    loge("%s: write error: %zd %s\n", __func__, bytes_sent, strerror(errno));
    return -1;
  }

  return 0;
}

static int unmap_camera_capture_buf(struct client_ctx *ctx)
{
  struct camera_capture_mem_info *mem_info = &(ctx->camera_buf);

  int rc = 0;

  if ((mem_info->data != NULL) && (mem_info->camera_capture_fd > 0) && (mem_info->ion_handle > 0)) {
    rc = munmap(mem_info->data, mem_info->size);
  }

  if (rc == -1) {
    loge("%s: failed to unmap ION mem: %s", __func__, strerror(errno));
  }

  mem_info->data = NULL;
  mem_info->size = 0;

  if (mem_info->camera_capture_fd > 0) {
    close(mem_info->camera_capture_fd);
    mem_info->camera_capture_fd = -1;
  }

  if (mem_info->ion_fd > 0) {
    struct ion_handle_data handle_data = {
      .handle = mem_info->ion_handle
    };
    rc = ioctl(mem_info->ion_fd, ION_IOC_FREE, &handle_data);
    if (rc == -1) {
      loge("%s: failed to free ION mem: %s", __func__, strerror(errno));
    }
    close(mem_info->ion_fd);
    mem_info->ion_fd = -1;
  }

  return rc;
}

static int mmap_camera_capture_buf(struct client_ctx *ctx)
{
  struct camera_capture_mem_info *mem_info = &(ctx->camera_buf);

  int main_ion_fd = open("/dev/ion", O_RDONLY);
  if (main_ion_fd == -1) {
    loge("%s: Ion dev open failed: %s", __func__, strerror(errno));
    goto ION_OPEN_FAILED;
  }

  // ion_share - import shared fd
  int rc = 0;
  struct ion_fd_data data = {
    .fd = mem_info->camera_capture_fd,
  };

  rc = ioctl(main_ion_fd, ION_IOC_IMPORT, &data);
  if (rc == -1) {
    loge("%s: Ion import failed: %s", __func__, strerror(errno));
    goto ION_IMPORT_FAILED;
  }

  size_t buf_size = mem_info->size;

  #if !defined(NDEBUG)
  // Buffer size should always be a multiple of 4K block size
  size_t buf_size_align = (buf_size + 4095U) & (~4095U);
  assert(buf_size == buf_size_align);
  #endif

  uint8_t *buf = mmap(NULL,
                      buf_size,
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED,
                      data.fd,
                      0);

  if (buf == MAP_FAILED) {
    loge("%s: ION mmap failed: %s", __func__, strerror(errno));
    goto ION_MAP_FAILED;
  }

  mem_info->ion_fd = main_ion_fd;
  mem_info->camera_capture_fd = data.fd;
  mem_info->ion_handle = data.handle;
  mem_info->data = buf;

  return 0;

ION_MAP_FAILED: {
    struct ion_handle_data handle_data = {
      .handle = data.handle
    };
    rc = ioctl(main_ion_fd, ION_IOC_FREE, &handle_data);
    if (rc == -1) {
      loge("%s: ION FREE failed: %s\n", __func__, strerror(errno));
    }
  }
ION_IMPORT_FAILED: {
    close(main_ion_fd);
  }
ION_OPEN_FAILED:
  return -1;
}

//
// manage client slot->frame mapping
//

// entries in locked_slots[] are 64bits.
// We store the frame_id as a 32-bit value, with bit32 indicating occupancy.
// Empty entries are set to zero.
static const uint64_t LOCKED_FLAG = 0x100000000ULL;
static const uint64_t VALUE_MASK = 0x000000000ffffffffULL;

static int add_locked_slot(struct client_ctx *ctx, uint8_t slot, uint32_t frame_id)
{
  assert(slot < ANKI_CAMERA_MAX_FRAME_COUNT);
  int rc = -1;
  if (slot < ANKI_CAMERA_MAX_FRAME_COUNT) {
    ctx->locked_slots[slot] = (frame_id | LOCKED_FLAG);
    rc = 0;
  }
  return rc;
}

// Tries to lock all slots
// locked_slots's LOCKED_FLAG mask will indicate if the lock
// was acquired for a given slot
static void lock_all_slots(struct client_ctx *ctx)
{
  uint8_t *data = ctx->camera_buf.data;
  if (data == NULL) {
    return;
  }

  anki_camera_buf_header_t *header = (anki_camera_buf_header_t *)data;

  for (uint32_t slot = 0; slot < ANKI_CAMERA_MAX_FRAME_COUNT; ++slot) {
    // lock slot
    uint32_t lock_status = 0;
    _Atomic uint32_t *slot_lock = &(header->locks.frame_locks[slot]);
    if (!atomic_compare_exchange_strong(slot_lock, &lock_status, 1)) {
      continue;
    }

    ctx->locked_slots[slot] |= LOCKED_FLAG;
  }
}

// Forcefully unlock all slots
static void unlock_all_slots(struct client_ctx *ctx)
{
  uint8_t *data = ctx->camera_buf.data;
  if (data == NULL) {
    return;
  }

  anki_camera_buf_header_t *header = (anki_camera_buf_header_t *)data;

  for (uint32_t slot = 0; slot < ANKI_CAMERA_MAX_FRAME_COUNT; ++slot) {
    // unlock slot
    uint32_t lock_status = 1;
    _Atomic uint32_t *slot_lock = &(header->locks.frame_locks[slot]);
    if (!atomic_compare_exchange_strong(slot_lock, &lock_status, 0)) {
      continue;
    }

    ctx->locked_slots[slot] = 0;
  }
}

// Unlock all slots that we have locked except for "slot"
static void unlock_slots_except(struct client_ctx *ctx, uint32_t slot)
{
  uint8_t *data = ctx->camera_buf.data;
  if (data == NULL) {
    return;
  }

  anki_camera_buf_header_t *header = (anki_camera_buf_header_t *)data;

  for(int i = 0; i < ANKI_CAMERA_MAX_FRAME_COUNT; i++)
  {
    if(i != slot && ctx->locked_slots[i] & LOCKED_FLAG)
    {
      _Atomic uint32_t *slot_lock = &(header->locks.frame_locks[i]);
      // unlock slot
      uint32_t lock_status = 1;
      if (!atomic_compare_exchange_strong(slot_lock, &lock_status, 0)) {
        loge("%s: could not unlock frame: %s", __func__, strerror(errno));
        continue;
      }

      ctx->locked_slots[i] &= ~LOCKED_FLAG;
    }
  }
}

static void unlock_slots(struct client_ctx *ctx)
{
  unlock_slots_except(ctx, ANKI_CAMERA_MAX_FRAME_COUNT+1);
}

static int get_locked_frame(struct client_ctx *ctx, uint32_t slot, uint32_t *out_frame_id)
{
  assert(slot < ANKI_CAMERA_MAX_FRAME_COUNT);
  int is_locked = -1;
  if (slot < ANKI_CAMERA_MAX_FRAME_COUNT) {
    uint64_t v = ctx->locked_slots[slot];
    if ((v & LOCKED_FLAG) == LOCKED_FLAG) {
      *out_frame_id = (uint32_t)(v & VALUE_MASK);
      is_locked = 0;
    }
  }
  return is_locked;
}

static int get_locked_slot(struct client_ctx *ctx, uint32_t frame_id, uint32_t *out_slot)
{
  int is_locked = -1;
  for (uint32_t slot = 0; slot < ANKI_CAMERA_MAX_FRAME_COUNT; ++slot) {
    uint64_t v = ctx->locked_slots[slot];
    if (v == (frame_id | LOCKED_FLAG)) {
      *out_slot = slot;
      is_locked = 0;
      break;
    }
  }
  return is_locked;
}

static int remove_locked_slot(struct client_ctx *ctx, uint32_t frame_id, uint32_t *out_slot)
{
  uint32_t slot;
  int rc = get_locked_slot(ctx, frame_id, &slot);
  if (rc == 0) {
    if (out_slot != NULL) {
      *out_slot = slot;
    }
    ctx->locked_slots[slot] = 0;
  }
  return rc;
}

static int write_outgoing_data(struct client_ctx *ctx)
{
  uint32_t i;
  int rc = 0;
  uint32_t msg_count = ctx->tx_cursor;
  for (i = 0; i < msg_count; ++i) {
    struct anki_camera_msg *msg = &(ctx->tx_packets[i]);
    rc = send_message(ctx, msg);
    logv("%s: send msg %d", __func__, msg->msg_id);
    ctx->tx_cursor--;
    if (rc != 0) {
      break;
    }
  }
  return rc;
}

static int enqueue_message(struct client_ctx *ctx, anki_camera_msg_id_t msg_id)
{
  pthread_mutex_lock(&ctx->ipc_mutex);
  uint32_t cursor = ctx->tx_cursor;

  const int TX_SIZE = (sizeof(ctx->tx_packets) / sizeof(ctx->tx_packets[0]));
  if(cursor+1 >= TX_SIZE)
  {
    pthread_mutex_unlock(&ctx->ipc_mutex);
    loge("%s: tx message buffer full, dropping message %d", __func__, msg_id);
    return -1;
  }

  struct anki_camera_msg *msg = &ctx->tx_packets[cursor];
  msg->msg_id = msg_id;
  ctx->tx_cursor = cursor + 1;
  pthread_mutex_unlock(&ctx->ipc_mutex);
  logv("%s: enqueue_message: %d", __func__, msg_id);
  return 0;
}

static int enqueue_message_with_payload(struct client_ctx *ctx, anki_camera_msg_id_t msg_id, void* buf, size_t len)
{
  pthread_mutex_lock(&ctx->ipc_mutex);
  uint32_t cursor = ctx->tx_cursor;

  const int TX_SIZE = (sizeof(ctx->tx_packets) / sizeof(ctx->tx_packets[0]));
  if(cursor+1 >= TX_SIZE)
  {
    pthread_mutex_unlock(&ctx->ipc_mutex);
    loge("%s: tx message buffer full, dropping message %d", __func__, msg_id);
    return -1;
  }

  struct anki_camera_msg *msg = &ctx->tx_packets[cursor];
  msg->msg_id = msg_id;

  size_t num = len;
  if(num > ANKI_CAMERA_MSG_PAYLOAD_LEN)
  {
    pthread_mutex_unlock(&ctx->ipc_mutex);
    loge("%s: enqueue_message payload size too large %u > %u", __func__, len, ANKI_CAMERA_MSG_PAYLOAD_LEN);
    return -1;
  }
  memcpy(msg->payload, buf, num);

  ctx->tx_cursor = cursor + 1;
  pthread_mutex_unlock(&ctx->ipc_mutex);
  logv("%s: enqueue_message: %d", __func__, msg_id);
  return 0;
}

static int process_one_message(struct client_ctx *ctx, struct anki_camera_msg *msg)
{
  int rc = 0;
  const anki_camera_msg_id_t msg_id = msg->msg_id;
  switch (msg_id) {
  case ANKI_CAMERA_MSG_S2C_STATUS: {
    anki_camera_msg_id_t ack_msg_id = msg->payload[0];
    logv("%s: received STATUS ack: %d\n", __func__, ack_msg_id);
    switch (ack_msg_id) {
    case ANKI_CAMERA_MSG_C2S_CLIENT_REGISTER: {
      ctx->status = ANKI_CAMERA_STATUS_IDLE;
    }
    break;
    case ANKI_CAMERA_MSG_C2S_CLIENT_UNREGISTER: {
      ctx->status = ANKI_CAMERA_STATUS_OFFLINE;
    }
    break;
    case ANKI_CAMERA_MSG_C2S_START: {
      ctx->status = ANKI_CAMERA_STATUS_RUNNING;
    }
    break;
    case ANKI_CAMERA_MSG_C2S_STOP: {
      ctx->status = ANKI_CAMERA_STATUS_IDLE;
    }
    break;
    default:
      break;
    }
  }
  break;
  case ANKI_CAMERA_MSG_S2C_BUFFER: {

    unlock_all_slots(ctx);

    // If we already have a fd then unmap it
    // since we are getting a new one
    if(ctx->camera_buf.camera_capture_fd > 0)
    {
      rc = unmap_camera_capture_buf(ctx);
      if(rc < 0)
      {
        loge("%s: ANKI_CAMERA_MSG_S2C_BUFFER unmap failed %d", __func__, rc);
      }
    }

    s_camera_handle.current_frame_id = UINT32_MAX;
    s_camera_handle.last_frame_slot = UINT32_MAX;

    // payload contains len
    uint32_t buffer_size;
    memcpy(&buffer_size, msg->payload, sizeof(buffer_size));
    logv("%s: received ANKI_CAMERA_MSG_S2C_BUFFER :: fd=%d size=%u\n",
         __func__, msg->fd, buffer_size);
    ctx->camera_buf.camera_capture_fd = msg->fd;
    ctx->camera_buf.size = buffer_size;
    rc = mmap_camera_capture_buf(ctx);
  }
  break;
  case ANKI_CAMERA_MSG_S2C_HEARTBEAT: {
    break;
  }
  break;
  default: {
    loge("%s: received unexpected message: %d\n", __func__, msg_id);
    rc = -1;
  }
  break;
  }
  return rc;
}

static int process_incoming_messages(struct client_ctx *ctx)
{
  uint32_t i;
  int rc = 0;
  uint32_t msg_count = ctx->rx_cursor;
  for (i = 0; i < msg_count; ++i) {
    struct anki_camera_msg *msg = &(ctx->rx_packets[i]);
    rc = process_one_message(ctx, msg);
    ctx->rx_cursor--;
    if (rc != 0) {
      break;
    }
  }
  return rc;
}

static int read_incoming_data(struct client_ctx *ctx)
{
  // Read all available data
  int rc = -1;
  do {
    // Check for space to receive data
    if (ctx->rx_cursor == ANKI_CAMERA_MAX_PACKETS) {
      loge("%s: No more space, dropping packet", __func__);
      rc = -1;
      break;
    }

    // Prepare rx buffer
    struct anki_camera_msg *msg = &(ctx->rx_packets[ctx->rx_cursor]);
    memset(msg, 0, sizeof(*msg));
    int recv_fd = -1;
    rc = read_fd(ctx->fd, msg, sizeof(*msg), &recv_fd);
    if (rc < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Expected normal case after reading all data
        rc = 0;
      }
      else {
        loge("%s: read failed: %s", __func__, strerror(errno));
        rc = -1; // indicate read failed
      }
    }
    else if (rc > 0) {
      // mark rx buffer slot as used
      ctx->rx_cursor++;

      if (recv_fd >= 0) {
        // If we received a file descriptor, store it
        msg->fd = recv_fd;
      }

      logv("%s: received msg:%d fd:%d\n", __func__, msg->msg_id, recv_fd);
    }
  }
  while (rc > 0);

  // Attempt to process any incoming messages
  // Should do nothing if we didn't receive any messages
  process_incoming_messages(ctx);

  return rc;
}

static int event_loop(struct client_ctx *ctx)
{
  struct timeval timeout;
  fd_set read_fds;
  fd_set write_fds;

  int fd = ctx->fd;
  int max_fd = fd;

  int rc = -1;
  do {
    FD_ZERO(&write_fds);
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    if (ctx->tx_cursor > 0) {
      FD_SET(ctx->fd, &write_fds);
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = HEARTBEAT_INTERVAL_US;

    rc = select(max_fd + 1, &read_fds, &write_fds, NULL, &timeout);

    if (rc == -1) {
      break;
    }

    if (rc == 0) {
      // select timeout
      break;
    }

    if (rc > 0) {
      if (FD_ISSET(fd, &write_fds)) {
        logv("%s: write %d\n", __func__, rc);
        rc = write_outgoing_data(ctx);
        if (rc == -1) {
          break;
        }
      }
      if (FD_ISSET(fd, &read_fds)) {
        logv("%s: read %d\n", __func__, rc);
        rc = read_incoming_data(ctx);
        if (rc == -1) {
          break;
        }
      }
    }
  }
  while (ctx->is_running);

  if(ctx->status == ANKI_CAMERA_STATUS_OFFLINE)
  {
    return -1;
  }

  return rc;
}

static void *camera_client_thread(void *camera_handle_ptr)
{
  logi("%s: start", __func__);
  struct anki_camera_handle_private *handle = (struct anki_camera_handle_private *)camera_handle_ptr;
  struct client_ctx *client = &handle->camera_client;

  client->status = ANKI_CAMERA_STATUS_IDLE;
  enqueue_message(client, ANKI_CAMERA_MSG_C2S_CLIENT_REGISTER);

  struct timespec lastHeartbeatTs = {0, 0};

  int rc = 0;
  while (client->status != ANKI_CAMERA_STATUS_OFFLINE) {
    // Process events or timeout after HEARTBEAT_INTERVAL
    rc = event_loop(client);
    if (rc == -1) {
      break;
    }

    // Only handle requests to start if we are idle and aren't waiting for a delete/shutdown
    // request to be completed
    if (client->status == ANKI_CAMERA_STATUS_IDLE && !client->waiting_for_delete) {
      if (client->request_start) {
        client->status = ANKI_CAMERA_STATUS_STARTING;
        enqueue_message(client, ANKI_CAMERA_MSG_C2S_START);
        client->request_start = 0;
      }
    }

    // send message to keep server alive
    struct timespec now = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint64_t elapsedNs = (now.tv_nsec + now.tv_sec * NSEC_PER_SEC) -
                         (lastHeartbeatTs.tv_nsec + lastHeartbeatTs.tv_sec * NSEC_PER_SEC);
    if (elapsedNs > HEARTBEAT_INTERVAL_NS) {
      enqueue_message(client, ANKI_CAMERA_MSG_C2S_HEARTBEAT);
      lastHeartbeatTs = now;
    }
  }

  client->status = ANKI_CAMERA_STATUS_OFFLINE;

  return NULL;
}

//
// Public API
//

#define CAMERA_HANDLE_P(camera) ((struct anki_camera_handle_private *)(camera))

// Initializes the camera
int camera_init(struct anki_camera_handle **camera)
{
  if(s_pwdn_gpio != NULL)
  {
    gpio_close(s_pwdn_gpio);
  }

  int res = gpio_create(PWDN_PIN, gpio_DIR_OUTPUT, gpio_LOW, &s_pwdn_gpio);
  if(res < 0)
  {
    loge("%s: failed to create pwdn gpio %d", __func__, res);
    return -1;
  }

  // configure logging
  setAndroidLoggingTag("anki-cam-client");

  memset(&s_camera_handle, 0, sizeof(s_camera_handle));

  struct client_ctx *client = &s_camera_handle.camera_client;
  client->fd = -1;
  client->camera_buf.camera_capture_fd = -1;
  client->camera_buf.ion_fd = -1;

  int rc = socket_connect(&(client->fd));
  if (rc != 0) {
    loge("%s: connection error: %s", __func__, strerror(errno));
    return -1;
  }

  pthread_mutex_init(&client->ipc_mutex, NULL);

  if (pthread_create(&client->ipc_thread, NULL, camera_client_thread, &s_camera_handle)) {
    loge("%s: error creating thread: %s", __func__, strerror(errno));
    return -1;
  }

  pthread_setname_np(client->ipc_thread, "EngCameraClient");

  client->is_running = 1;
  s_camera_handle.current_frame_id = UINT32_MAX;
  s_camera_handle.last_frame_slot = UINT32_MAX;
  *camera = (struct anki_camera_handle *)&s_camera_handle;

  return 0;
}

// Starts capturing frames in new thread, sends them to callback `cb`.
int camera_start(struct anki_camera_handle *camera)
{
  // Start
  // received ack, start camera
  struct client_ctx *client = &CAMERA_HANDLE_P(camera)->camera_client;
  client->request_start = 1;
  return 0;
}

// Stops capturing frames
int camera_stop(struct anki_camera_handle *camera)
{
  // Stop
  enqueue_message(&CAMERA_HANDLE_P(camera)->camera_client, ANKI_CAMERA_MSG_C2S_STOP);
  return 0;
}

void camera_pause(struct anki_camera_handle *camera, int pause)
{
  struct client_ctx *client = &CAMERA_HANDLE_P(camera)->camera_client;

  const enum Gpio_Level value = (pause == 0 ? gpio_LOW : gpio_HIGH);
  int res = gpio_set_value(s_pwdn_gpio, value);
  if(res < 0)
  {
    // VIC-12258 981 fault code randomly occuring
    // This is assuming gpio_set_value failed due to the gpio no longer existing
    // I think either mm-anki-camera or mm-qcamera-daemon unexports it when it cleans up
    // the open stream. What might be happening is one of those processes is crashing and being
    // being automatically restarted (we handle this and can recover).
    // During the crash gpio94 is unexported so we fail to set its value and bring the
    // camera out of standby. This results in engine no longer receiving frames and eventually
    // showing the 981 fault code.
    // If this is the issue then it should be fixable by simply recreating gpio94.
    fprintf(stderr, "camera_pause %d Failed to set gpio %d, recreating\n", pause, errno);
    gpio_close(s_pwdn_gpio);
    res = gpio_create(PWDN_PIN, gpio_DIR_OUTPUT, value, &s_pwdn_gpio);
    if(res < 0)
    {
      fprintf(stderr, "camera_pause Failed to recreate gpio, camera left in previous pause state\n");
      return;
    }
  }

  if(pause)
  {
    // Camera is being paused so all existing images should be marked as invalid
    // so lock all slots and set frame timestamp to 0
    // Keep slots locked until camera is unpaused
    lock_all_slots(client);

    uint8_t *data = client->camera_buf.data;
    if (data == NULL) {
      return;
    }
    anki_camera_buf_header_t *header = (anki_camera_buf_header_t *)data;

    for(uint32_t slot = 0; slot < ANKI_CAMERA_MAX_FRAME_COUNT; slot++)
    {
      const uint32_t frame_offset = header->frame_offsets[slot];
      anki_camera_frame_t *frame = (anki_camera_frame_t *)&data[frame_offset];
      if(frame != NULL)
      {
        frame->timestamp = 0;
      }
    }
  }
  else
  {
    // Camera is being unpaused so unlock all slots so images can be captured
    unlock_all_slots(client);
  }
}

// De-initializes camera, makes it available to rest of system
int camera_release(struct anki_camera_handle *camera)
{
  CAMERA_HANDLE_P(camera)->camera_client.waiting_for_delete = 1;
  enqueue_message(&CAMERA_HANDLE_P(camera)->camera_client, ANKI_CAMERA_MSG_C2S_CLIENT_UNREGISTER);
  return 0;
}

int camera_destroy(struct anki_camera_handle* camera)
{
  int rc = pthread_tryjoin_np(CAMERA_HANDLE_P(camera)->camera_client.ipc_thread, NULL);
  if(rc != 0)
  {
    return 0;
  }

  pthread_mutex_destroy(&CAMERA_HANDLE_P(camera)->camera_client.ipc_mutex);

  struct client_ctx *client = &CAMERA_HANDLE_P(camera)->camera_client;

  // close socket
  if (client->fd >= 0) {
    close(client->fd);
    client->fd = -1;
  }

  // unmap & free ion mem
  rc = unmap_camera_capture_buf(client);
  if (rc != 0) {
    loge("%s: error unmapping capture buffer", __func__);
  }

  client->waiting_for_delete = 0;

  return 1;
}

// Attempt (lock) the last available frame for reading
int camera_frame_acquire(struct anki_camera_handle *camera,
                         uint64_t frame_timestamp,
                         anki_camera_frame_t **out_frame)
{
  //assert(camera != NULL);
  if(camera == NULL)
  {
    loge("%s: camera is null", __func__);
    return -1;
  }

  int rc = 0;
  struct client_ctx *client = &CAMERA_HANDLE_P(camera)->camera_client;
  uint8_t *data = client->camera_buf.data;
  if (data == NULL) {
    return -1;
  }
  anki_camera_buf_header_t *header = (anki_camera_buf_header_t *)data;

  // Lock all slots so we can iterate over them and find the one
  // that has a timestamp closest to or before frame_timestamp
  lock_all_slots(client);

  // Start with the most recently written frame slot
  uint32_t wSlot = atomic_load(&header->locks.write_idx);
  if(wSlot >= ANKI_CAMERA_MAX_FRAME_COUNT)
  {
    loge("%s: invalid write_idx %u", __func__, wSlot);
    unlock_slots(client);
    return -1;
  }
  else if(wSlot == CAMERA_HANDLE_P(camera)->last_frame_slot)
  {
    unlock_slots(client);
    return -1;
  }

  // Keep track of which slot has the best timestamp
  uint64_t bestTime = 0;
  uint32_t bestSlot = wSlot;

  for(uint32_t slot = 0; slot < ANKI_CAMERA_MAX_FRAME_COUNT; slot++)
  {
    uint32_t fid = 0;
    // Make sure this is a slot that we locked
    // Don't want to be checking a slot the camera server is currently modifying
    int is_locked = get_locked_frame(client, slot, &fid);
    if (is_locked) {
      continue;
    }

    const uint32_t frame_offset = header->frame_offsets[slot];
    anki_camera_frame_t *frame = (anki_camera_frame_t *)&data[frame_offset];

    // Skip any slots that have null frames or invalid timestamps
    if(frame == NULL ||
       frame->timestamp == 0)
    {
      continue;
    }

    // If this frame's timestamp is at or before frame_timestamp and
    // it is newer than bestTime
    if(frame->timestamp <= frame_timestamp &&
       frame->timestamp > bestTime)
    {
      bestSlot = slot;
      bestTime = frame->timestamp;
    }
  }

  // lock best slot for reading
  uint32_t slot = bestSlot;

  const uint32_t frame_offset = header->frame_offsets[slot];
  anki_camera_frame_t *frame = (anki_camera_frame_t *)&data[frame_offset];

  if(frame == NULL)
  {
    loge("%s: frame is null", __func__);
    rc = -1;
    goto UNLOCK;
  }

  if (frame->frame_id == CAMERA_HANDLE_P(camera)->current_frame_id) {
    //logw("%s: duplicate frame: %u\n", __func__, frame->frame_id);
    rc = -1;
    goto UNLOCK;
  }

  if(frame->timestamp == 0)
  {
    logd("%s: %u has zero timestamp", __func__, slot);
    rc = -1;
    goto UNLOCK;
  }

  CAMERA_HANDLE_P(camera)->current_frame_id = frame->frame_id;
  CAMERA_HANDLE_P(camera)->last_frame_slot = slot;

  // Add this frame to the locked slot
  add_locked_slot(client, slot, frame->frame_id);

  // Unlock the rest of the slots
  unlock_slots_except(client, slot);

  if (out_frame != NULL) {
    *out_frame = frame;
  }

  return rc;

UNLOCK:

  unlock_slots(client);
  return rc;
}

// Release (unlock) frame to camera system
int camera_frame_release(struct anki_camera_handle *camera, uint32_t frame_id)
{
  int rc = 0;
  struct client_ctx *client = &CAMERA_HANDLE_P(camera)->camera_client;
  uint8_t *data = client->camera_buf.data;
  if (data == NULL) {
    return -1;
  }
  anki_camera_buf_header_t *header = (anki_camera_buf_header_t *)data;

  // Lookup slot;
  uint32_t slot;
  rc = get_locked_slot(client, frame_id, &slot);
  if (rc == -1) {
    // Not really an error, just means someone asked us to release a frame we
    // don't know about
    logd("%s: failed to find slot for frame_id %u", __func__, frame_id);
    return 0;
  }

  // unlock slot
  uint32_t lock_status = 1;
  _Atomic uint32_t *slot_lock = &(header->locks.frame_locks[slot]);
  if (!atomic_compare_exchange_strong(slot_lock, &lock_status, 0)) {
    loge("%s: could not unlock frame (slot: %u): %s", __func__, slot, strerror(errno));
    rc = -1;
  }

  rc = remove_locked_slot(client, frame_id, NULL);

  return rc;
}

anki_camera_status_t camera_status(struct anki_camera_handle *camera)
{
  if(camera == NULL)
  {
    return ANKI_CAMERA_STATUS_OFFLINE;
  }

  struct client_ctx *client = &CAMERA_HANDLE_P(camera)->camera_client;
  if (client == NULL) {
    return ANKI_CAMERA_STATUS_OFFLINE;
  }
  else {
    return client->status;
  }
}

int camera_set_exposure(struct anki_camera_handle* camera, uint16_t exposure_ms, float gain)
{
  anki_camera_exposure_t exposure;
  exposure.exposure_ms = exposure_ms;
  exposure.gain = gain;

  anki_camera_msg_params_payload_t payload;
  payload.id = ANKI_CAMERA_MSG_C2S_PARAMS_ID_EXP;
  memcpy(payload.data, &exposure, sizeof(exposure));

  return enqueue_message_with_payload(&CAMERA_HANDLE_P(camera)->camera_client,
                                      ANKI_CAMERA_MSG_C2S_PARAMS, &payload, sizeof(payload));
}

int camera_set_awb(struct anki_camera_handle* camera, float r_gain, float g_gain, float b_gain)
{
  anki_camera_awb_t awb;
  awb.r_gain = r_gain;
  awb.g_gain = g_gain;
  awb.b_gain = b_gain;

  anki_camera_msg_params_payload_t payload;
  payload.id = ANKI_CAMERA_MSG_C2S_PARAMS_ID_AWB;
  memcpy(payload.data, &awb, sizeof(awb));

  return enqueue_message_with_payload(&CAMERA_HANDLE_P(camera)->camera_client,
                                      ANKI_CAMERA_MSG_C2S_PARAMS, &payload, sizeof(payload));
}

int camera_set_capture_format(struct anki_camera_handle* camera, anki_camera_pixel_format_t format)
{
  // Lock all slots to prevent access to the shared memory that is
  // going to be deallocated by changing the capture format
  lock_all_slots(&CAMERA_HANDLE_P(camera)->camera_client);

  anki_camera_msg_params_payload_t payload;
  payload.id = ANKI_CAMERA_MSG_C2S_PARAMS_ID_FORMAT;
  memcpy(payload.data, &format, sizeof(format));

  return enqueue_message_with_payload(&CAMERA_HANDLE_P(camera)->camera_client,
                                      ANKI_CAMERA_MSG_C2S_PARAMS, &payload, sizeof(payload));
}

int camera_set_capture_snapshot(struct anki_camera_handle* camera,
                                uint8_t start)
{
  anki_camera_msg_params_payload_t payload;
  payload.id = ANKI_CAMERA_MSG_C2S_PARAMS_ID_SNAPSHOT;
  memcpy(payload.data, &start, sizeof(start));

  return enqueue_message_with_payload(&CAMERA_HANDLE_P(camera)->camera_client,
                                      ANKI_CAMERA_MSG_C2S_PARAMS, &payload, sizeof(payload));
}
