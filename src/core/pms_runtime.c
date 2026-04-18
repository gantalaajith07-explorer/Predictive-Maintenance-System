#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/neutrino.h>
#include <unistd.h>

#include "pms_runtime.h"

static void pms_init_process_shared_mutex(pthread_mutex_t *lock) {
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(lock, &attr);
    pthread_mutexattr_destroy(&attr);
}

static void pms_init_process_shared_cond(pthread_cond_t *cond) {
    pthread_condattr_t attr;

    pthread_condattr_init(&attr);
    pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(cond, &attr);
    pthread_condattr_destroy(&attr);
}

int pms_runtime_create(PmsRuntime **runtime_out, int inject_faults) {
    PmsRuntime *runtime;

    runtime = mmap(NULL, sizeof(PmsRuntime), PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANON, -1, 0);
    if (runtime == MAP_FAILED) {
        perror("mmap");
        return -1;
    }

    memset(runtime, 0, sizeof(PmsRuntime));
    runtime->router_chid = -1;
    runtime->timer_chid = -1;

    pms_init_process_shared_mutex(&runtime->lock);
    pms_init_process_shared_cond(&runtime->prediction_ready);
    pthread_barrier_init(&runtime->start_barrier, NULL, PMS_START_BARRIER_COUNT);

    runtime->keep_running = 1;
    runtime->inject_faults = inject_faults;
    runtime->router_chid = ChannelCreate(_NTO_CHF_COID_DISCONNECT | _NTO_CHF_DISCONNECT);
    runtime->timer_chid = ChannelCreate(_NTO_CHF_UNBLOCK);

    if (runtime->router_chid < 0 || runtime->timer_chid < 0) {
        perror("ChannelCreate");
        pms_runtime_destroy(runtime);
        return -1;
    }

    *runtime_out = runtime;
    return 0;
}

void pms_runtime_request_stop(PmsRuntime *runtime) {
    int coid;
    int timer_coid;
    PmsIpcRequest stop_msg;
    PmsIpcReply reply;

    if (!runtime) {
        return;
    }

    pthread_mutex_lock(&runtime->lock);
    runtime->keep_running = 0;
    pthread_cond_broadcast(&runtime->prediction_ready);
    pthread_mutex_unlock(&runtime->lock);

    memset(&stop_msg, 0, sizeof(stop_msg));
    stop_msg.type = PMS_MSG_STOP;
    coid = ConnectAttach(0, 0, runtime->router_chid, _NTO_SIDE_CHANNEL, 0);
    if (coid >= 0) {
        MsgSend(coid, &stop_msg, sizeof(stop_msg), &reply, sizeof(reply));
        ConnectDetach(coid);
    }

    timer_coid = ConnectAttach(0, 0, runtime->timer_chid, _NTO_SIDE_CHANNEL, 0);
    if (timer_coid >= 0) {
        MsgSendPulse(timer_coid, SIGEV_PULSE_PRIO_INHERIT, PMS_MSG_STOP, 0);
        ConnectDetach(timer_coid);
    }
}

void pms_runtime_destroy(PmsRuntime *runtime) {
    if (!runtime) {
        return;
    }

    if (runtime->router_chid >= 0) {
        ChannelDestroy(runtime->router_chid);
    }
    if (runtime->timer_chid >= 0) {
        ChannelDestroy(runtime->timer_chid);
    }

    pthread_barrier_destroy(&runtime->start_barrier);
    pthread_cond_destroy(&runtime->prediction_ready);
    pthread_mutex_destroy(&runtime->lock);
    munmap(runtime, sizeof(PmsRuntime));
}
