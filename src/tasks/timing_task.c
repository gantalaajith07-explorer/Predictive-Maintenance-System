#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/neutrino.h>
#include <time.h>

#include "pms_runtime.h"

static int pms_arm_high_resolution_timer(PmsRuntime *runtime, timer_t *timer_id) {
    struct sigevent event;
    struct itimerspec spec;

    SIGEV_PULSE_INIT(&event, 0, SIGEV_PULSE_PRIO_INHERIT, _PULSE_CODE_MINAVAIL, 1);
    event.sigev_coid = ConnectAttach(0, 0, runtime->timer_chid, _NTO_SIDE_CHANNEL, 0);
    if (event.sigev_coid < 0) {
        return -1;
    }

    if (timer_create(CLOCK_MONOTONIC, &event, timer_id) != 0) {
        ConnectDetach(event.sigev_coid);
        return -1;
    }

    spec.it_value.tv_sec = 0;
    spec.it_value.tv_nsec = PMS_TIMER_TICK_MS * 1000000L;
    spec.it_interval = spec.it_value;
    timer_settime(*timer_id, 0, &spec, NULL);
    return event.sigev_coid;
}

static void pms_explain_interrupt_path(void) {
    printf("[IRQ] Hardware is ignored for this round; timer pulses model the same event-delivery path an ISR would wake.\n");
    printf("[IRQ] On a board, this task is where InterruptAttachEvent() would replace the simulated source.\n");
}

void *pms_timing_task(void *arg) {
    PmsRuntime *runtime = (PmsRuntime *) arg;
    timer_t timer_id;
    int timer_coid;
    int rcvid;
    struct _pulse pulse;

    pms_apply_thread_priority(PMS_PRIO_TIMING, SCHED_FIFO, "TIMER");
    pthread_barrier_wait(&runtime->start_barrier);
    pms_explain_interrupt_path();

    timer_coid = pms_arm_high_resolution_timer(runtime, &timer_id);
    if (timer_coid < 0) {
        printf("[TIMER] Could not arm timer: %s\n", strerror(errno));
        return NULL;
    }

    printf("[TIMER] High-resolution timer delivers pulses every %d ms.\n", PMS_TIMER_TICK_MS);
    while (runtime->keep_running) {
        uint64_t timeout_ns = 2ULL * PMS_TIMER_TICK_MS * 1000000ULL;

        TimerTimeout(CLOCK_MONOTONIC, _NTO_TIMEOUT_RECEIVE, NULL, &timeout_ns, NULL);
        rcvid = MsgReceive(runtime->timer_chid, &pulse, sizeof(pulse), NULL);
        if (rcvid == 0) {
            pthread_mutex_lock(&runtime->lock);
            runtime->evidence.timer_ns = pms_monotonic_ns();
            runtime->evidence.pulses_seen++;
            pthread_mutex_unlock(&runtime->lock);
        } else if (rcvid == -1 && errno == ETIMEDOUT) {
            pthread_mutex_lock(&runtime->lock);
            runtime->evidence.timeouts_seen++;
            pthread_mutex_unlock(&runtime->lock);
        }
    }

    timer_delete(timer_id);
    ConnectDetach(timer_coid);
    printf("[TIMER] Timer, pulse connection, and kernel timeout path cleaned up.\n");
    return NULL;
}
