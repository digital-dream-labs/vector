#include <stdio.h>
#include <unistd.h>

#include "camera_client.h"


int run_camera_client(struct anki_camera_handle *camera)
{
    fprintf(stderr, "run_camera_client: E\n");
    int rc = camera_init(&camera);
    fprintf(stderr, "initialized camera\n");
    if (rc != 0) {
        return rc;
    }

    usleep(100000);
    fprintf(stderr, "attempt to start camera\n");
    rc = camera_start(camera);
    if (rc != 0) {
        return rc;
    }

    // wait for camera to start
    while ( camera_status(camera) != ANKI_CAMERA_STATUS_RUNNING ) {
        usleep(30000);
    }

    int r = 0;
    while(camera_status(camera) == ANKI_CAMERA_STATUS_RUNNING) {
        usleep(60000);
        anki_camera_frame_t* frame;
        r = camera_frame_acquire(camera, &frame);
        if (r != 0) {
            continue;
        }
        fprintf(stderr, "received frame: %u\n", frame->frame_id);
        r = camera_frame_release(camera, frame->frame_id);
    }

    fprintf(stderr, "camera_stop\n");
    (void) camera_stop(camera);
    fprintf(stderr, "camera_release\n");
    (void) camera_release(camera);

    fprintf(stderr, "run_camera_client: X\n");
    return rc;
}

int main(int argc, char* argv[])
{
    struct anki_camera_handle *camera;

    int rc = 0;
    while(1) {
        fprintf(stderr, "%s\n", "run_camera_client: start");
        rc = run_camera_client(camera);
        if (rc != 0) {
            fprintf(stderr, "error running client: %d\n", rc);
        }
        usleep(100000);
        fprintf(stderr, "%s\n", "run_camera_client: exit");
    }

    return rc;
}
