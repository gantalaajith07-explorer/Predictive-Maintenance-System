\# Predictive Maintenance System — QNX RTOS Concepts



!\[Platform](https://img.shields.io/badge/Platform-QNX%20Neutrino-blue)

!\[Language](https://img.shields.io/badge/Language-C-lightgrey)

!\[Concepts](https://img.shields.io/badge/RTOS-IPC%20%7C%20Threads%20%7C%20Timers%20%7C%20Scheduling-green)

!\[License](https://img.shields.io/badge/License-Academic-orange)



\---



\## What is this project?



A \*\*Predictive Maintenance System (PMS)\*\* built to demonstrate QNX Neutrino RTOS concepts.  

It simulates industrial sensor data (vibration, temperature, acceleration), runs a health  

prediction algorithm, and streams live results to a Python dashboard — all using real QNX APIs.



\---



\## QNX Concepts Demonstrated



| Concept | Where Used |

|--------|------------|

| Microkernel IPC (MsgSend / MsgReceive / MsgReply) | `pms\_ipc\_router.c` |

| QNX Channels (ChannelCreate / ConnectAttach) | `pms\_runtime.c`, `acquisition\_task.c` |

| Priority Scheduling (SCHED\_FIFO / SCHED\_RR) | `pms\_scheduler.c`, all tasks |

| POSIX Threads (pthread\_create) | `app\_boot.c` |

| Mutex (pthread\_mutex\_t) | `pms\_runtime.c`, all tasks |

| Condition Variable (pthread\_cond\_timedwait) | `notification\_task.c` |

| Barrier (pthread\_barrier\_wait) | `app\_boot.c`, all tasks |

| Shared Memory (mmap MAP\_SHARED) | `pms\_runtime.c` |

| fork() — Logger process | `logger\_process.c` |

| spawn() — Supervisor watchdog | `supervisor\_child.c`, `app\_boot.c` |

| POSIX Timer (timer\_create CLOCK\_MONOTONIC) | `timing\_task.c` |

| QNX Pulse Events (SIGEV\_PULSE\_INIT) | `timing\_task.c` |

| TimerTimeout() — kernel timeout guard | `timing\_task.c` |

| sigwait() — clean shutdown | `app\_boot.c` |

| Process-shared synchronization primitives | `pms\_runtime.c` |



\---



\## Project Structure



PMS\_QNX\_Concepts/

├── src/

│   ├── app\_boot.c              # main() — orchestrates startup \& shutdown

│   ├── pms\_contract.h          # shared types, constants, message structs

│   ├── pms\_runtime.h           # function declarations

│   ├── core/

│   │   ├── pms\_model.c         # health prediction algorithm

│   │   ├── pms\_runtime.c       # shared memory, channels, sync primitives

│   │   └── pms\_scheduler.c     # priority scheduling, clock utilities

│   ├── ipc/

│   │   └── pms\_ipc\_router.c    # QNX microkernel IPC server

│   ├── tasks/

│   │   ├── acquisition\_task.c  # sensor data producer (MsgSend)

│   │   ├── notification\_task.c # condvar consumer + TCP dashboard

│   │   └── timing\_task.c       # high-resolution timer + pulse events

│   └── processes/

│       ├── logger\_process.c    # fork() child — CSV logger

│       └── supervisor\_child.c  # spawn() watchdog process

├── python/

│   └── dashboard\_live.py       # live web dashboard (Plotly Dash)

├── docs/

│   └── QNX\_CONCEPT\_MAP.md      # concept reference table

├── Makefile

├── README.md

├── run\_dashboard.bat           # Windows dashboard launcher

└── run\_dashboard.sh            # Linux dashboard launcher



\---



\## How Data Flows



acquisition\_task  →  MsgSend()  →  pms\_ipc\_router  →  pms\_model (prediction)

↓

pthread\_cond\_broadcast()

↓

notification\_task  →  TCP  →  Python Dashboard

↓

logger\_process (fork)  →  CSV file



\---



\## How to Build and Run



\### Requirements

\- QNX Momentics IDE

\- QNX SDP 7.x or 8.x

\- Python 3.x with `dash`, `plotly`, `pandas` (for dashboard)



\### Build

```bash

\# Import project in QNX Momentics

\# Build target: x86\_64-debug

make

```



\### Run

```bash

\# Normal mode

./pms\_qnx\_concepts



\# Fault injection mode (accelerates damage progression)

./pms\_qnx\_concepts --fault

```



\### Dashboard

```bash

\# Windows

run\_dashboard.bat



\# Linux / QNX host

./run\_dashboard.sh

```

Then open `http://127.0.0.1:8050` in your browser.



\---



\## Priority Table



| Thread / Process | Priority | Policy |

|-----------------|----------|--------|

| timing\_task | 26 | SCHED\_FIFO |

| pms\_ipc\_router | 24 | SCHED\_FIFO |

| notification\_task | 21 | SCHED\_FIFO |

| acquisition\_task | 18 | SCHED\_RR |

| main (app\_boot) | 16 | SCHED\_RR |



\---



\## Author



\*\*Gantala Ajith\*\* — QNX RTOS Concepts Project

