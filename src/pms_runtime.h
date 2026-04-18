#ifndef PMS_RUNTIME_H
#define PMS_RUNTIME_H

#include "pms_contract.h"

int pms_runtime_create(PmsRuntime **runtime_out, int inject_faults);
void pms_runtime_request_stop(PmsRuntime *runtime);
void pms_runtime_destroy(PmsRuntime *runtime);

int pms_apply_process_priority(int priority, int policy, const char *tag);
int pms_apply_thread_priority(int priority, int policy, const char *tag);
void pms_print_clock_notes(void);
void pms_print_multicore_notes(void);

void *pms_ipc_router(void *arg);
void *pms_acquisition_task(void *arg);
void *pms_notification_task(void *arg);
void *pms_timing_task(void *arg);

pid_t pms_start_logger_process(PmsRuntime *runtime);
void pms_logger_process(PmsRuntime *runtime);

#endif
