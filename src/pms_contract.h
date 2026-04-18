#ifndef PMS_CONTRACT_H
#define PMS_CONTRACT_H

#include <pthread.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/neutrino.h>
#include <time.h>

#define PMS_NAME                 "PMS_QNX_CONCEPTS"
#define PMS_DASHBOARD_PORT       8888
#define PMS_START_BARRIER_COUNT  5

#define PMS_PRIO_ACQUIRE         18
#define PMS_PRIO_ROUTER          24
#define PMS_PRIO_NOTIFY          21
#define PMS_PRIO_TIMING          26
#define PMS_PRIO_MAIN            16

#define PMS_ACQUIRE_MS           100
#define PMS_NOTIFY_TIMEOUT_MS    750
#define PMS_TIMER_TICK_MS        250
#define PMS_SHUTDOWN_GRACE_MS    500

#define PMS_VIB_WARN_G           2.6f
#define PMS_VIB_CRIT_G           4.4f
#define PMS_TEMP_WARN_C          55.0f
#define PMS_TEMP_CRIT_C          70.0f
#define PMS_ACCEL_WARN_G         3.0f
#define PMS_ACCEL_CRIT_G         5.5f

typedef enum {
    PMS_STATE_NORMAL = 0,
    PMS_STATE_WARNING = 1,
    PMS_STATE_CRITICAL = 2
} PmsState;

typedef enum {
    PMS_MSG_SENSOR_SAMPLE = 1,
    PMS_MSG_GET_SNAPSHOT = 2,
    PMS_MSG_STOP = 3
} PmsMessageType;

typedef struct {
    float vibration_g;
    float temperature_c;
    float accel_x_g;
    float accel_y_g;
    float accel_z_g;
    float accel_rms_g;
    uint32_t sample_id;
    uint64_t captured_ns;
} PmsSensorFrame;

typedef struct {
    PmsState state;
    float health_pct;
    float remaining_hours;
    float confidence_pct;
    uint64_t decided_ns;
} PmsPredictionFrame;

typedef struct {
    PmsMessageType type;
    PmsSensorFrame sensor;
} PmsIpcRequest;

typedef struct {
    int status;
    PmsSensorFrame sensor;
    PmsPredictionFrame prediction;
} PmsIpcReply;

typedef struct {
    uint64_t acquisition_ns;
    uint64_t router_ns;
    uint64_t notify_ns;
    uint64_t timer_ns;
    uint32_t ipc_round_trips;
    uint32_t pulses_seen;
    uint32_t timeouts_seen;
} PmsEvidence;

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t prediction_ready;
    pthread_barrier_t start_barrier;

    PmsSensorFrame sensor;
    PmsPredictionFrame prediction;
    PmsEvidence evidence;

    int keep_running;
    int inject_faults;
    int router_chid;
    int timer_chid;
    pid_t logger_pid;
    pid_t supervisor_pid;
} PmsRuntime;

static inline uint64_t pms_monotonic_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t) ts.tv_sec * 1000000000ULL) + (uint64_t) ts.tv_nsec;
}

const char *pms_state_text(PmsState state);
void pms_predict(const PmsSensorFrame *sensor, PmsPredictionFrame *prediction);

#endif
