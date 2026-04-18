#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "pms_runtime.h"

pid_t pms_start_logger_process(PmsRuntime *runtime) {
    pid_t pid;

    pid = fork();
    if (pid == 0) {
        pms_logger_process(runtime);
        _exit(0);
    }
    return pid;
}

void pms_logger_process(PmsRuntime *runtime) {
    FILE *log;
    struct timespec sleep_time;
    uint32_t last_sample = 0;

    mkdir("/tmp/pms_qnx_concepts", 0755);
    log = fopen("/tmp/pms_qnx_concepts/pms_trace.csv", "a");
    if (!log) {
        return;
    }

    fprintf(log, "sample,time_ns,state,health,rul,ipc_round_trips,pulses,timeouts\n");
    sleep_time.tv_sec = 0;
    sleep_time.tv_nsec = 300000000L;

    while (runtime->keep_running) {
        PmsSensorFrame sensor;
        PmsPredictionFrame prediction;
        PmsEvidence evidence;

        pthread_mutex_lock(&runtime->lock);
        sensor = runtime->sensor;
        prediction = runtime->prediction;
        evidence = runtime->evidence;
        pthread_mutex_unlock(&runtime->lock);

        if (sensor.sample_id != last_sample) {
            last_sample = sensor.sample_id;
            fprintf(log, "%u,%llu,%s,%.2f,%.2f,%u,%u,%u\n",
                sensor.sample_id,
                (unsigned long long) prediction.decided_ns,
                pms_state_text(prediction.state),
                prediction.health_pct,
                prediction.remaining_hours,
                evidence.ipc_round_trips,
                evidence.pulses_seen,
                evidence.timeouts_seen);
            fflush(log);
        }

        nanosleep(&sleep_time, NULL);
    }

    fclose(log);
}
