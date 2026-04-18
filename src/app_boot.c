#include <pthread.h>
#include <process.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "pms_runtime.h"

static PmsRuntime *g_runtime = NULL;

static int pms_launch_thread(pthread_t *thread, void *(*entry)(void *), PmsRuntime *runtime) {
    pthread_attr_t attr;
    int rc;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 64 * 1024);
    rc = pthread_create(thread, &attr, entry, runtime);
    pthread_attr_destroy(&attr);
    return rc;
}

static pid_t pms_spawn_supervisor_child(const char *argv0) {
    char parent_arg[32];
    char child_path[256];
    char *child_argv[3];
    pid_t child_pid;
    const char *slash;

    slash = strrchr(argv0, '/');
    if (slash) {
        size_t prefix_len = (size_t) (slash - argv0 + 1);
        snprintf(child_path, sizeof(child_path), "%.*spms_supervisor_child", (int) prefix_len, argv0);
    } else {
        snprintf(child_path, sizeof(child_path), "./pms_supervisor_child");
    }

    snprintf(parent_arg, sizeof(parent_arg), "%d", getpid());
    child_argv[0] = child_path;
    child_argv[1] = parent_arg;
    child_argv[2] = NULL;

    child_pid = spawnv(P_NOWAIT, child_path, child_argv);
    if (child_pid == -1) {
        perror("[BOOT] spawnv");
        return -1;
    }

    return child_pid;
}

int main(int argc, char *argv[]) {
    pthread_t router_thread;
    pthread_t acquisition_thread;
    pthread_t notification_thread;
    pthread_t timer_thread;
    sigset_t stop_set;
    int received_signal;
    int inject_faults = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--fault") == 0) {
            inject_faults = 1;
        }
    }

    printf("============================================================\n");
    printf(" Predictive Maintenance System - QNX Concepts Build\n");
    printf("============================================================\n");
    printf("Focus: microkernel IPC, priority scheduling, timing, cleanup.\n\n");

    pms_apply_process_priority(PMS_PRIO_MAIN, SCHED_RR, "BOOT");
    pms_print_clock_notes();
    pms_print_multicore_notes();

    sigemptyset(&stop_set);
    sigaddset(&stop_set, SIGINT);
    sigaddset(&stop_set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &stop_set, NULL);

    if (pms_runtime_create(&g_runtime, inject_faults) != 0) {
        return 1;
    }

    g_runtime->logger_pid = pms_start_logger_process(g_runtime);
    if (g_runtime->logger_pid > 0) {
        printf("[BOOT] fork() logger pid=%d.\n", g_runtime->logger_pid);
    }

    g_runtime->supervisor_pid = pms_spawn_supervisor_child(argv[0]);
    if (g_runtime->supervisor_pid > 0) {
        printf("[BOOT] spawn() supervisor pid=%d.\n", g_runtime->supervisor_pid);
    }

    pms_launch_thread(&router_thread, pms_ipc_router, g_runtime);
    pms_launch_thread(&acquisition_thread, pms_acquisition_task, g_runtime);
    pms_launch_thread(&notification_thread, pms_notification_task, g_runtime);
    pms_launch_thread(&timer_thread, pms_timing_task, g_runtime);

    printf("[BOOT] Waiting at pthread_barrier until core services are ready.\n");
    pthread_barrier_wait(&g_runtime->start_barrier);
    printf("[BOOT] All services released together. Press Ctrl+C to stop.\n\n");

    sigwait(&stop_set, &received_signal);
    printf("[BOOT] Signal %d received through sigwait(). Starting controlled shutdown.\n", received_signal);
    pms_runtime_request_stop(g_runtime);

    pthread_join(acquisition_thread, NULL);
    pthread_join(notification_thread, NULL);
    pthread_join(timer_thread, NULL);
    pthread_join(router_thread, NULL);

    if (g_runtime->logger_pid > 0) {
        waitpid(g_runtime->logger_pid, NULL, 0);
    }
    if (g_runtime->supervisor_pid > 0) {
        kill(g_runtime->supervisor_pid, SIGTERM);
        waitpid(g_runtime->supervisor_pid, NULL, 0);
    }

    pms_runtime_destroy(g_runtime);
    g_runtime = NULL;
    printf("[BOOT] Process termination and cleanup complete.\n");
    return 0;
}
