#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/neutrino.h>

#include "pms_runtime.h"

void *pms_ipc_router(void *arg) {
    PmsRuntime *runtime = (PmsRuntime *) arg;
    PmsIpcRequest request;
    PmsIpcReply reply;
    int rcvid;

    pms_apply_thread_priority(PMS_PRIO_ROUTER, SCHED_FIFO, "IPC");
    pthread_barrier_wait(&runtime->start_barrier);
    printf("[IPC] ChannelCreate id=%d. Waiting with MsgReceive().\n", runtime->router_chid);

    while (runtime->keep_running) {
        rcvid = MsgReceive(runtime->router_chid, &request, sizeof(request), NULL);
        if (rcvid == -1) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        memset(&reply, 0, sizeof(reply));
        if (rcvid == 0) {
            continue;
        }

        if (request.type == PMS_MSG_STOP) {
            reply.status = 0;
            MsgReply(rcvid, 0, &reply, sizeof(reply));
            break;
        }

        if (request.type == PMS_MSG_SENSOR_SAMPLE) {
            pms_predict(&request.sensor, &reply.prediction);
            reply.sensor = request.sensor;
            reply.status = 0;

            pthread_mutex_lock(&runtime->lock);
            runtime->sensor = request.sensor;
            runtime->prediction = reply.prediction;
            runtime->evidence.router_ns = reply.prediction.decided_ns;
            runtime->evidence.ipc_round_trips++;
            pthread_cond_broadcast(&runtime->prediction_ready);
            pthread_mutex_unlock(&runtime->lock);

            MsgReply(rcvid, 0, &reply, sizeof(reply));
            continue;
        }

        if (request.type == PMS_MSG_GET_SNAPSHOT) {
            pthread_mutex_lock(&runtime->lock);
            reply.sensor = runtime->sensor;
            reply.prediction = runtime->prediction;
            pthread_mutex_unlock(&runtime->lock);
            reply.status = 0;
            MsgReply(rcvid, 0, &reply, sizeof(reply));
            continue;
        }

        reply.status = -1;
        MsgReply(rcvid, EINVAL, &reply, sizeof(reply));
    }

    printf("[IPC] Router stopped. MsgReply() finished pending work before exit.\n");
    return NULL;
}
