#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "pms_runtime.h"

static int g_dashboard_client = -1;

static int pms_open_dashboard_socket(void) {
    struct sockaddr_in addr;
    int fd;
    int reuse = 1;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PMS_DASHBOARD_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }
    listen(fd, 1);
    fcntl(fd, F_SETFL, O_NONBLOCK);
    return fd;
}

static void pms_accept_dashboard(int server_fd) {
    int client_fd;

    if (server_fd < 0) {
        return;
    }

    client_fd = accept(server_fd, NULL, NULL);
    if (client_fd >= 0) {
        if (g_dashboard_client >= 0) {
            close(g_dashboard_client);
        }
        g_dashboard_client = client_fd;
        printf("[NOTIFY] Python dashboard connected.\n");
    }
}

static void pms_stream_dashboard(const PmsSensorFrame *sensor, const PmsPredictionFrame *prediction, const PmsEvidence *evidence) {
    char line[512];
    int length;

    if (g_dashboard_client < 0) {
        return;
    }

    length = snprintf(line, sizeof(line),
        "{\"sample\":%u,\"state\":\"%s\",\"vib\":%.3f,\"temp\":%.2f,"
        "\"accel\":%.3f,\"health\":%.2f,\"rul\":%.2f,\"conf\":%.2f,"
        "\"ipc\":%u,\"pulses\":%u,\"timeouts\":%u,"
        "\"wd_sensor\":%d,\"wd_ipc\":%d,\"wd_alert\":%d,\"wd_timer\":%d,"
        "\"rs_sensor\":%u,\"rs_ipc\":%u,\"rs_alert\":%u,\"rs_timer\":%u}\n",
        sensor->sample_id,
        pms_state_text(prediction->state),
        sensor->vibration_g,
        sensor->temperature_c,
        sensor->accel_rms_g,
        prediction->health_pct,
        prediction->remaining_hours,
        prediction->confidence_pct,
        evidence->ipc_round_trips,
        evidence->pulses_seen,
        evidence->timeouts_seen,
        evidence->acquisition_ns > 0,
        evidence->router_ns > 0,
        evidence->notify_ns > 0,
        evidence->timer_ns > 0,
        0U,
        0U,
        0U,
        evidence->timeouts_seen);

    if (send(g_dashboard_client, line, length, MSG_NOSIGNAL) < 0) {
        close(g_dashboard_client);
        g_dashboard_client = -1;
        printf("[NOTIFY] Python dashboard disconnected.\n");
    }
}

static void pms_emit_console_alert(const PmsSensorFrame *sensor, const PmsPredictionFrame *prediction) {
    printf("[NOTIFY] sample=%u state=%s health=%.1f%% rul=%.1fh confidence=%.0f%%\n",
        sensor->sample_id,
        pms_state_text(prediction->state),
        prediction->health_pct,
        prediction->remaining_hours,
        prediction->confidence_pct);
}

void *pms_notification_task(void *arg) {
    PmsRuntime *runtime = (PmsRuntime *) arg;
    struct timespec wait_until;
    int server_fd;
    uint32_t last_sample = 0;

    pms_apply_thread_priority(PMS_PRIO_NOTIFY, SCHED_FIFO, "NOTIFY");
    pthread_barrier_wait(&runtime->start_barrier);

    server_fd = pms_open_dashboard_socket();
    if (server_fd >= 0) {
        printf("[NOTIFY] Dashboard endpoint is listening on TCP %d.\n", PMS_DASHBOARD_PORT);
    } else {
        printf("[NOTIFY] Dashboard socket skipped: %s\n", strerror(errno));
    }

    while (runtime->keep_running) {
        PmsSensorFrame sensor;
        PmsPredictionFrame prediction;
        PmsEvidence evidence;

        pms_accept_dashboard(server_fd);

        clock_gettime(CLOCK_REALTIME, &wait_until);
        wait_until.tv_nsec += PMS_NOTIFY_TIMEOUT_MS * 1000000L;
        if (wait_until.tv_nsec >= 1000000000L) {
            wait_until.tv_sec++;
            wait_until.tv_nsec -= 1000000000L;
        }

        pthread_mutex_lock(&runtime->lock);
        while (runtime->keep_running && runtime->sensor.sample_id == last_sample) {
            if (pthread_cond_timedwait(&runtime->prediction_ready, &runtime->lock, &wait_until) == ETIMEDOUT) {
                runtime->evidence.timeouts_seen++;
                break;
            }
        }
        sensor = runtime->sensor;
        prediction = runtime->prediction;
        evidence = runtime->evidence;
        runtime->evidence.notify_ns = pms_monotonic_ns();
        pthread_mutex_unlock(&runtime->lock);

        if (!runtime->keep_running) {
            break;
        }

        if (sensor.sample_id != last_sample) {
            last_sample = sensor.sample_id;
            pms_emit_console_alert(&sensor, &prediction);
            pms_stream_dashboard(&sensor, &prediction, &evidence);
        }
    }

    if (g_dashboard_client >= 0) {
        close(g_dashboard_client);
    }
    if (server_fd >= 0) {
        close(server_fd);
    }
    printf("[NOTIFY] Mutex/condvar notification task cleaned up.\n");
    return NULL;
}
