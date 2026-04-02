#include "policy.h"
#include "utils.h"
#include "enforce.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define CPU_THRESHOLD 30.0
#define ALPHA 0.3 



#define SYSTEM_CPU_THRESHOLD 80.0
#define FG_CPU_MIN 20.0



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

    list->total_cpu = 0.0;
    float fg_cpu = 0.0;

    for (int i = 0; i < list->count; i++) {
        process_t *p = &list->processes[i];

        list->total_cpu += p->cpu_usage;

        if (p->foreground)
            fg_cpu += p->cpu_usage;

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

    list->avg_cpu = (ALPHA * list->total_cpu) + ((1 - ALPHA) * list->avg_cpu);

    list->system_stress = (list->avg_cpu > SYSTEM_CPU_THRESHOLD);

    for (int i = 0; i < group_count; i++) {

        pid_t tgid = tgids[i];
        float proc_cpu = cpu_sum[i];

        if (tgid == GOVERNOR_PID) continue;

        process_t *rep = NULL;
        for (int k = 0; k < list->count; k++) {
            if (list->processes[k].tgid == tgid) {
                rep = &list->processes[k];
                break;
            }
        }

        if (!rep) continue;

        if(rep->state == STATE_FROZEN) continue;

        if (rep->foreground && rep->state!=STATE_FROZEN) {
            rep->violations = 0;
            rep->baseline_cpu = 0.0;
            rep->was_throttled = 0;

            for (int k = 0; k < list->count; k++) {
                if (list->processes[k].tgid == tgid) {
                    list->processes[k].state = STATE_NORMAL;
                }
            }
            continue;
        }

        if (!list->system_stress && rep->state != STATE_FROZEN) {
            rep->violations = 0;
            rep->baseline_cpu = 0.0;
            rep->was_throttled = 0;

            for (int k = 0; k < list->count; k++) {
                if (list->processes[k].tgid == tgid) {
                    list->processes[k].state = STATE_NORMAL;
                }
            }
            continue;
        }

        if(rep->state!=STATE_FROZEN){
            if (proc_cpu > CPU_THRESHOLD) {
                rep->violations++;
            } else {
                rep->violations -= 2;
                if (rep->violations < 0) rep->violations = 0;
            }
        }


        if (rep->violations >= VIOLATION_L3){
            if (rep->state != STATE_FROZEN) {
                rep->frozen_since = time(NULL);
            }
            rep->state = STATE_FROZEN;
            enforce_freeze(rep);
        }
        else if (rep->violations >= VIOLATION_L2){
            rep->state = STATE_HARD_THROTTLED;
        }
        else if (rep->violations >= VIOLATION_L1){
            rep->state = STATE_THROTTLED;
        }
        else{
            rep->state = STATE_NORMAL;
        }

        if (rep->state == STATE_THROTTLED && rep->was_throttled == 0) {
            rep->baseline_cpu = proc_cpu;
            rep->was_throttled = 1;
        }

        if (rep->state == STATE_NORMAL) {
            rep->was_throttled = 0;
            rep->baseline_cpu = 0.0;
        }

        for (int k = 0; k < list->count; k++) {
            if (list->processes[k].tgid == tgid) {
                list->processes[k].state = rep->state;
            }
        }
    }

    free(tgids);
    free(cpu_sum);
}