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

    pid_t fg_tgid = -1;
    if (fg_pid > 0) {
        process_t *fg_proc = find_process(list, fg_pid);
        if (fg_proc) {
            fg_tgid = fg_proc->tgid;
        }
    }

    // Mark all threads with that tgid as foreground, others background
    for (int i = 0; i < list->count; i++) {
        if (fg_tgid != -1 && list->processes[i].tgid == fg_tgid) {
            list->processes[i].foreground = 1;
        } else {
            list->processes[i].foreground = 0;
        }
    }
}

void apply_policy(process_list_t *list) {
    int n = list->count;

    pid_t *tgids = malloc(n * sizeof(pid_t));
    float *cpu_sum = calloc(n, sizeof(float));
    int group_count = 0;

    for (int i = 0; i < list->count; i++) {
        process_t *p = &list->processes[i];

        int found = -1;

        for (int j = 0; j < group_count; j++) {
            if (tgids[j] == p->tgid) {
                found = j;
                break;
            }
        }

        if (found == -1) {
            tgids[group_count] = p->tgid;
            cpu_sum[group_count] = p->cpu_usage;
            group_count++;
        } else {
            cpu_sum[found] += p->cpu_usage;
        }
    }

    for (int i = 0; i < group_count; i++) {

        pid_t tgid = tgids[i];
        float total_cpu = cpu_sum[i];

        // Find a representative thread for this process
        process_t *rep = NULL;

        for (int k = 0; k < list->count; k++) {
            if (list->processes[k].tgid == tgid) {
                rep = &list->processes[k];
                break;
            }
        }

        if (!rep) continue;

        // If the process is foreground, reset and skip throttling
        if (rep->foreground) {
            for (int k = 0; k < list->count; k++) {
                if (list->processes[k].tgid == tgid) {
                    list->processes[k].violations = 0;
                    list->processes[k].state = STATE_NORMAL;
                }
            }
            continue;
        }

        // Correct violation tracking
        if (total_cpu > CPU_THRESHOLD) {
            rep->violations++;
        } else if (rep->violations > 0) {
            rep->violations--;
        }

        // Decide state
        int state;
        if (rep->violations >= VIOLATION_LIMIT)
            state = STATE_THROTTLED;
        else
            state = STATE_NORMAL;

        // Apply to ALL threads of this process
        for (int k = 0; k < list->count; k++) {
            if (list->processes[k].tgid == tgid) {
                list->processes[k].state = state;
            }
        }
    }

    free(tgids);
    free(cpu_sum);  
}