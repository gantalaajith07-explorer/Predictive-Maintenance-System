# Predictive Maintenance System - QNX Momentics

Simple QNX Momentics project for a Predictive Maintenance System.  
Hardware is skipped for now. Sensors are simulated. Main focus is QNX RTOS
concepts.

## What It Does

- Reads simulated vibration, temperature, and acceleration values.
- Predicts machine state: `NORMAL`, `WARNING`, or `CRITICAL`.
- Sends alerts to console and Python dashboard.
- Logs results for demo evidence.

## QNX Concepts Included

- Mutex
- Condvar
- Barrier
- Priority scheduling
- IPC and message passing
- `ChannelCreate()`
- `MsgReceive()`
- `MsgReply()`
- `MsgSend()`
- Event delivery using pulses
- Interrupt-handling style using timer pulse events
- Multi-core scheduling notes
- Shared memory using `mmap(MAP_SHARED)`
- `fork()`
- `spawn()`
- Process termination and cleanup
- System clock reading
- Timers and high-resolution timers
- Kernel timeout using `TimerTimeout()`

## Build In QNX Momentics

1. Open QNX Momentics.
2. Import existing project:
   `C:\Users\pilli\OneDrive\Documents\PMS_QNX_Concepts`
3. Select `x86_64-debug`.
4. Build.

## Run QNX App

```sh
./build/x86_64-debug/pms_qnx_concepts
./build/x86_64-debug/pms_qnx_concepts --fault
```

Use `--fault` for faster warning and critical states.

## Run Dashboard

On Windows:

```bat
run_dashboard.bat
```

Then open:

```text
http://127.0.0.1:8050
```

The dashboard runs in demo mode if QNX is not connected. When the QNX app runs,
it receives live JSON data from TCP port `8888`.

Dashboard components:

- status bar
- vibration, temperature, and acceleration charts
- health trend
- RUL and health gauges
- watchdog monitor
- alert log

## Main Files

- `src/app_boot.c`
- `src/core/pms_runtime.c`
- `src/ipc/pms_ipc_router.c`
- `src/tasks/acquisition_task.c`
- `src/tasks/notification_task.c`
- `src/tasks/timing_task.c`
- `python/dashboard_live.py`
- `docs/QNX_CONCEPT_MAP.md`
