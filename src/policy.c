#include "policy.h"
#include "utils.h"
#include "enforce.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define CPU_THRESHOLD 30.0

#define VIOLATION_L1 2
#define VIOLATION_L2 4
#define VIOLATION_L3 6

#define SYSTEM_CPU_THRESHOLD 80.0
#define FG_CPU_MIN 20.0

// Get active window ID
unsigned long get_active_window() {
    FILE *fp = popen("xdotool getwindowfocus 2>/dev/null", "r");
    if (!fp) return 0;

    unsigned long win = 0;
    fscanf(fp, "%lu", &win);
    pclose(fp);
    return win;
}

// Get PID from window ID
pid_t get_pid_from_window(unsigned long win) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "xdotool getwindowpid %lu 2>/dev/null", win);

    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    pid_t pid = -1;
    fscanf(fp, "%d", &pid);
    pclose(fp);
    return pid;
}


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

    float total_cpu = 0.0;
    float fg_cpu = 0.0;

    // 🔹 Aggregate CPU per process (tgid)
    for (int i = 0; i < list->count; i++) {
        process_t *p = &list->processes[i];

        total_cpu += p->cpu_usage;

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

    // 🔥 Foreground starvation detection
    list->system_stress = (total_cpu > SYSTEM_CPU_THRESHOLD && fg_cpu < FG_CPU_MIN);

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

        // ✅ Foreground always protected
        if (rep->foreground && rep->state!=STATE_FROZEN) {
            rep->violations = 0;

            for (int k = 0; k < list->count; k++) {
                if (list->processes[k].tgid == tgid) {
                    list->processes[k].state = STATE_NORMAL;
                }
            }
            continue;
        }

        // ❗ If system not stressed → do nothing
        if (!list->system_stress && rep->state != STATE_FROZEN) {
            rep->violations = 0;

            for (int k = 0; k < list->count; k++) {
                if (list->processes[k].tgid == tgid) {
                    list->processes[k].state = STATE_NORMAL;
                }
            }
            continue;
        }

        // 🔹 Violation logic
        if(rep->state!=STATE_FROZEN){
            if (proc_cpu > CPU_THRESHOLD) {
                rep->violations++;
            } else {
                rep->violations -= 2;
                if (rep->violations < 0) rep->violations = 0;
            }
        }

        // 🔹 Multi-level state decision

        if (rep->violations >= VIOLATION_L3){
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

        // Apply to all threads
        for (int k = 0; k < list->count; k++) {
            if (list->processes[k].tgid == tgid) {
                list->processes[k].state = rep->state;
            }
        }
    }

    free(tgids);
    free(cpu_sum);
}