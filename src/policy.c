#include "policy.h"
#include <stdio.h>
#include <stdlib.h>

#define CPU_THRESHOLD 30.0
#define VIOLATION_LIMIT 2

pid_t get_foreground_pid() {

    FILE *fp = popen("xdotool getwindowfocus getwindowpid 2>/dev/null", "r");

    if (!fp)
        return -1;

    pid_t pid = -1;

    fscanf(fp, "%d", &pid);

    pclose(fp);

    return pid;
}

void update_foreground_status(process_list_t *list, pid_t fg_pid) {

    for (int i = 0; i < list->count; i++) {

        if (list->processes[i].pid == fg_pid)
            list->processes[i].foreground = 1;
        else
            list->processes[i].foreground = 0;
    }
}

void apply_policy(process_list_t *list) {

    for (int i = 0; i < list->count; i++) {

        process_t *p = &list->processes[i];

        if (p->foreground) {
            p->state = STATE_NORMAL;
            p->violation_count = 0;
            continue;
        }

        float avg = 0;

        for (int j = 0; j < HISTORY_SIZE; j++)
            avg += p->history[j];

        avg /= HISTORY_SIZE;

        if (avg > CPU_THRESHOLD)
            p->violation_count++;
        else if (p->violation_count > 0)
            p->violation_count--;

        if (p->violation_count >= VIOLATION_LIMIT)
            p->state = STATE_THROTTLED;
        else
            p->state = STATE_NORMAL;
    }
}