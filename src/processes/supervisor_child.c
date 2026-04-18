#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int parent_pid;
    struct timespec period;

    if (argc < 2) {
        fprintf(stderr, "usage: pms_supervisor_child <parent-pid>\n");
        return 2;
    }

    parent_pid = atoi(argv[1]);
    period.tv_sec = 1;
    period.tv_nsec = 0;

    printf("[SUPERVISOR-CHILD] spawn() launched watchdog for parent pid %d.\n", parent_pid);
    while (kill(parent_pid, 0) == 0) {
        nanosleep(&period, NULL);
    }

    printf("[SUPERVISOR-CHILD] Parent ended, child cleanup complete.\n");
    return 0;
}
