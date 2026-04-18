#include <math.h>
#include <stdlib.h>

#include "pms_contract.h"

#define PMS_HISTORY 16

static float g_health_history[PMS_HISTORY];
static int g_cursor = 0;
static int g_count = 0;

const char *pms_state_text(PmsState state) {
    static const char *names[] = {"NORMAL", "WARNING", "CRITICAL"};
    if (state < PMS_STATE_NORMAL || state > PMS_STATE_CRITICAL) {
        return "UNKNOWN";
    }
    return names[state];
}

static float pms_limit(float value, float low, float high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static void pms_remember_health(float health) {
    g_health_history[g_cursor] = health;
    g_cursor = (g_cursor + 1) % PMS_HISTORY;
    if (g_count < PMS_HISTORY) {
        g_count++;
    }
}

static float pms_health_slope(void) {
    float first;
    float last;

    if (g_count < 4) {
        return 0.0f;
    }

    first = g_health_history[(g_cursor - g_count + PMS_HISTORY) % PMS_HISTORY];
    last = g_health_history[(g_cursor - 1 + PMS_HISTORY) % PMS_HISTORY];
    return (last - first) / (float) g_count;
}

void pms_predict(const PmsSensorFrame *sensor, PmsPredictionFrame *prediction) {
    float vib_score;
    float temp_score;
    float accel_score;
    float slope;

    vib_score = 100.0f - ((sensor->vibration_g / PMS_VIB_CRIT_G) * 100.0f);
    temp_score = 100.0f - ((sensor->temperature_c / PMS_TEMP_CRIT_C) * 100.0f);
    accel_score = 100.0f - ((sensor->accel_rms_g / PMS_ACCEL_CRIT_G) * 100.0f);

    prediction->health_pct = (0.42f * pms_limit(vib_score, 0.0f, 100.0f)) +
        (0.35f * pms_limit(temp_score, 0.0f, 100.0f)) +
        (0.23f * pms_limit(accel_score, 0.0f, 100.0f));

    if (sensor->vibration_g >= PMS_VIB_CRIT_G ||
        sensor->temperature_c >= PMS_TEMP_CRIT_C ||
        sensor->accel_rms_g >= PMS_ACCEL_CRIT_G) {
        prediction->state = PMS_STATE_CRITICAL;
    } else if (sensor->vibration_g >= PMS_VIB_WARN_G ||
        sensor->temperature_c >= PMS_TEMP_WARN_C ||
        sensor->accel_rms_g >= PMS_ACCEL_WARN_G) {
        prediction->state = PMS_STATE_WARNING;
    } else {
        prediction->state = PMS_STATE_NORMAL;
    }

    pms_remember_health(prediction->health_pct);
    slope = pms_health_slope();
    prediction->remaining_hours = 500.0f;
    if (slope < -0.01f) {
        prediction->remaining_hours = pms_limit((prediction->health_pct - 10.0f) / fabsf(slope) * 0.01f, 0.0f, 500.0f);
    }

    prediction->confidence_pct = pms_limit(55.0f + fabsf(70.0f - prediction->health_pct), 55.0f, 99.0f);
    prediction->decided_ns = pms_monotonic_ns();
}
