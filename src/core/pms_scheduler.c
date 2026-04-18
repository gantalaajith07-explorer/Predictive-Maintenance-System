#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/neutrino.h>
#include <time.h>
#include <unistd.h>

#include "pms_runtime.h"

int pms_apply_process_priority(int priority, int policy, const char *tag) {
    struct sched_param param;

    memset(&param, 0, sizeof(param));
    param.sched_priority = priority;
    if (sched_setscheduler(0, policy, &param) != 0) {
        printf("[%s] Priority request %d failed: %s\n", tag, priority, strerror(errno));
        return -1;
    }

    printf("[%s] Process priority=%d policy=%s\n",
        tag,
        priority,
        policy == SCHED_FIFO ? "SCHED_FIFO" : "SCHED_RR");
    return 0;
}

int pms_apply_thread_priority(int priority, int policy, const char *tag) {
    pthread_t self;
    struct sched_param param;

    self = pthread_self();
    memset(&param, 0, sizeof(param));
    param.sched_priority = priority;
    if (pthread_setschedparam(self, policy, &param) != 0) {
        printf("[%s] Thread priority request %d failed.\n", tag, priority);
        return -1;
    }

    printf("[%s] Thread priority=%d policy=%s\n",
        tag,
        priority,
        policy == SCHED_FIFO ? "SCHED_FIFO" : "SCHED_RR");
    return 0;
}

void pms_print_clock_notes(void) {
    struct timespec realtime;
    struct timespec monotonic;
    struct timespec resolution;

    clock_gettime(CLOCK_REALTIME, &realtime);
    clock_gettime(CLOCK_MONOTONIC, &monotonic);
    clock_getres(CLOCK_MONOTONIC, &resolution);

    printf("[TIME] CLOCK_REALTIME seconds=%ld. Set with ClockTime() only when system policy allows it.\n",
        (long) realtime.tv_sec);
    printf("[TIME] CLOCK_MONOTONIC ns=%llu, resolution=%ld ns.\n",
        (unsigned long long) (((uint64_t) monotonic.tv_sec * 1000000000ULL) + monotonic.tv_nsec),
        (long) resolution.tv_nsec);
}

void pms_print_multicore_notes(void) {
    long cpu_count;

    cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count < 1) {
        cpu_count = 1;
    }

    printf("[SCHED] Detected %ld CPU core(s). Demo uses priorities first; runmask/cluster pinning can be added per target BSP.\n",
        cpu_count);
}
