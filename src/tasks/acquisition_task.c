#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/neutrino.h>
#include <time.h>
#include <unistd.h>

#include "pms_runtime.h"

static float pms_random_between(float low, float high) {
    return low + ((float) rand() / (float) RAND_MAX) * (high - low);
}

static void pms_make_sensor_frame(PmsSensorFrame *frame, uint32_t sample_id, int inject_faults) {
    float damage;
    float swing;

    damage = inject_faults ? (float) (sample_id % 80) / 60.0f : (float) (sample_id % 200) / 500.0f;
    if (damage > 1.0f) {
        damage = 1.0f;
    }

    swing = 1.0f + (damage * 4.0f);
    memset(frame, 0, sizeof(*frame));
    frame->sample_id = sample_id;
    frame->vibration_g = fabsf(0.7f + damage * 4.1f + pms_random_between(-0.25f, 0.35f));
    frame->temperature_c = 35.0f + damage * 38.0f + pms_random_between(-1.0f, 1.5f);
    frame->accel_x_g = pms_random_between(-swing, swing) * 0.45f;
    frame->accel_y_g = pms_random_between(-swing, swing) * 0.45f;
    frame->accel_z_g = 1.0f + pms_random_between(-0.2f, 0.2f);
    frame->accel_rms_g = sqrtf((frame->accel_x_g * frame->accel_x_g) +
        (frame->accel_y_g * frame->accel_y_g) +
        (frame->accel_z_g * frame->accel_z_g));
    frame->captured_ns = pms_monotonic_ns();
}

void *pms_acquisition_task(void *arg) {
    PmsRuntime *runtime = (PmsRuntime *) arg;
    struct timespec period;
    uint32_t sample_id = 0;
    int coid;

    pms_apply_thread_priority(PMS_PRIO_ACQUIRE, SCHED_RR, "ACQUIRE");
    pthread_barrier_wait(&runtime->start_barrier);

    period.tv_sec = 0;
    period.tv_nsec = PMS_ACQUIRE_MS * 1000000L;
    coid = ConnectAttach(0, 0, runtime->router_chid, _NTO_SIDE_CHANNEL, 0);
    if (coid < 0) {
        perror("[ACQUIRE] ConnectAttach");
        return NULL;
    }

    printf("[ACQUIRE] Connected to IPC channel. Sending samples with MsgSend().\n");
    while (runtime->keep_running) {
        PmsIpcRequest request;
        PmsIpcReply reply;

        pms_make_sensor_frame(&request.sensor, ++sample_id, runtime->inject_faults);
        request.type = PMS_MSG_SENSOR_SAMPLE;

        if (MsgSend(coid, &request, sizeof(request), &reply, sizeof(reply)) == 0) {
            pthread_mutex_lock(&runtime->lock);
            runtime->evidence.acquisition_ns = request.sensor.captured_ns;
            pthread_mutex_unlock(&runtime->lock);
        }

        if (sample_id % 20 == 0) {
            printf("[ACQUIRE] #%u vibration=%.2fg temp=%.1fC accel=%.2fg\n",
                sample_id,
                request.sensor.vibration_g,
                request.sensor.temperature_c,
                request.sensor.accel_rms_g);
        }

        nanosleep(&period, NULL);
    }

    ConnectDetach(coid);
    printf("[ACQUIRE] Clean process termination path reached.\n");
    return NULL;
}
