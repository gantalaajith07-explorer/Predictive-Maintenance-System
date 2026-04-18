# QNX Concept Map

This project is a predictive maintenance system with simulated sensor data.
The main focus is QNX concepts, not hardware.

## Concepts Used

| QNX / RTOS concept | Used in project |
| --- | --- |
| Mutex | Protects shared runtime data |
| Condvar | Wakes alert/dashboard task |
| Barrier | Starts main services together |
| Priority scheduling | Different priorities for timer, IPC, alert, acquisition |
| IPC | Sensor task talks to analytics router |
| Channel create | `ChannelCreate()` creates QNX channels |
| Message passing | `MsgSend()`, `MsgReceive()`, `MsgReply()` |
| Event delivery | Timer sends QNX pulse events |
| Interrupt handling idea | Timer pulse simulates interrupt-style event delivery |
| Multi-core scheduling | CPU count is printed; runmask can be added on target |
| Shared memory | `mmap(MAP_SHARED)` for logger process |
| `fork()` | Starts CSV logger |
| `spawn()` | Starts supervisor process |
| Cleanup | Joins threads, waits processes, deletes timer, destroys channels |
| Timers | Periodic acquisition and dashboard update |
| High-resolution timer | `timer_create(CLOCK_MONOTONIC, ...)` |
| Kernel timeout | `TimerTimeout()` around `MsgReceive()` |

## Demo Flow

1. Import project in QNX Momentics.
2. Build `x86_64-debug`.
3. Run `pms_qnx_concepts --fault`.
4. Run `run_dashboard.bat` on Windows.
5. Open `http://127.0.0.1:8050`.

The dashboard runs with demo data if QNX is not connected, and switches to live
QNX stream when the app is available.
