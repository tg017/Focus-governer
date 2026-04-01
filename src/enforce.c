#include "enforce.h"
#include "process.h"
#include "utils.h"
#include "policy.h"
#include <sys/resource.h>
#include <sys/stat.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define CPU_THRESHOLD 30.0

int is_safe_to_throttle(pid_t pid) {
    if (pid < 100) return 0;
    if (pid == GOVERNOR_PID) return 0;

    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    FILE *f = fopen(path, "r");
    if (f) {
        char cmdline[256];
        size_t len = fread(cmdline, 1, sizeof(cmdline)-1, f);
        fclose(f);
        cmdline[len] = '\0';
        if (len == 0) return 0;
    }
    return 1;
}

void boost_foreground(process_t *proc) {
    if (!proc) return;

    if (setpriority(PRIO_PROCESS, proc->tgid, -10) == 0) {
        // log_action("BOOST", proc->pid, proc->name);
    }
}

void enforce_nice(process_t *proc) {
    if (!is_safe_to_throttle(proc->pid)) return;
    errno = 0;
    if (setpriority(PRIO_PROCESS, proc->pid, 19) == 0) {
        log_action("NICE", proc->pid, proc->name);
    } else if (errno != ESRCH) {
        // ESRCH means process exited – ignore
        char msg[256];
        snprintf(msg, sizeof(msg), "setpriority failed: %s", strerror(errno));
        log_message("ERROR", msg);
    }
}


void enforce_cgroup(process_t *proc) {
    if (!is_safe_to_throttle(proc->pid)) return;

    char path[256];
    snprintf(path, sizeof(path), "/sys/fs/cgroup/focus_%d", proc->tgid);

    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0755);

        char cpu_max[256];
        snprintf(cpu_max, sizeof(cpu_max), "%s/cpu.max", path);

        FILE *f = fopen(cpu_max, "w");
        if (f) {
            fprintf(f, "50000 100000");
            fclose(f);
        }
    }

    char procs[256];
    snprintf(procs, sizeof(procs), "%s/cgroup.procs", path);

    FILE *f = fopen(procs, "w");
    if (f) {
        fprintf(f, "%d", proc->tgid);
        fclose(f);
    }

    log_action("CGROUP", proc->pid, proc->name);
}

void enforce_freeze(process_t *proc) {
    if (!is_safe_to_throttle(proc->pid)) return;
    if (proc->state != STATE_FROZEN) return;

    if (kill(proc->pid, SIGSTOP) == 0) {
        log_action("FREEZE", proc->pid, proc->name);
    } else if (errno != ESRCH) {
        char msg[256];
        snprintf(msg, sizeof(msg), "kill SIGSTOP failed: %s", strerror(errno));
        log_message("ERROR", msg);
    }
}

void enforce_wake(process_t *proc) {
    if (!is_safe_to_throttle(proc->pid)) return;
    if (kill(proc->pid, SIGCONT) == 0) {
        log_action("WAKE", proc->pid, proc->name);
        // After waking, reset state to NORMAL (policy will re-evaluate)
        proc->state = STATE_NORMAL;
        proc->was_throttled = 0;
        proc->baseline_cpu = 0.0;
        cleanup_cgroup(proc);
    } else if (errno != ESRCH) {
        char msg[256];
        snprintf(msg, sizeof(msg), "kill SIGCONT failed: %s", strerror(errno));
        log_message("ERROR", msg);
    }
}

void cleanup_cgroup(process_t *proc) {
    if (!is_safe_to_throttle(proc->pid)) return;

    char path[256];
    snprintf(path, sizeof(path), "/sys/fs/cgroup/focus_%d", proc->tgid);

    struct stat st;
    if (stat(path, &st) == -1) return;  // cgroup doesn't exist → nothing to do

    FILE *f = fopen("/sys/fs/cgroup/cgroup.procs", "w");
    if (f) {
        fprintf(f, "%d", proc->tgid);
        fclose(f);
    }

    if (rmdir(path) == 0) {
        log_action("CGROUP_CLEANUP", proc->pid, proc->name);
    } else if (errno != ENOENT) {
        char msg[256];
        snprintf(msg, sizeof(msg), "cleanup failed: %s", strerror(errno));
        log_message("ERROR", msg);
    }
}

void apply_enforcement(process_list_t *list) {
    process_t *fg_proc = NULL;
    float saved_this_second = 0.0;

    for (int i = 0; i < list->count; i++) {
        process_t *p = &list->processes[i];

        if (p->pid == p->tgid && p->foreground) {
            fg_proc = p;
            break;
        }
    }

    for (int i = 0; i < list->count; i++) {
        process_t *p = &list->processes[i];

        if (fg_proc && list->system_stress) {
            boost_foreground(fg_proc);
        }

        if (p->pid != p->tgid) continue;
        if (p->tgid == GOVERNOR_PID) continue;

        if (p->was_throttled && p->baseline_cpu > 0) {

            float current = p->cpu_usage;

            float saved = p->baseline_cpu - current;

            if (saved > 0) {
                saved_this_second += saved / 100.0; 
            }
        }

        if (p->state == STATE_FROZEN) {
            if(p->foreground) enforce_wake(p);
            continue;
        }

        if (p->foreground) continue;

        switch (p->state) {
            case STATE_NORMAL:
                cleanup_cgroup(p);
                break;
            case STATE_THROTTLED:
                enforce_nice(p);
                //debug to check freezing
                // enforce_freeze(p);
                break;
            case STATE_HARD_THROTTLED:
                enforce_cgroup(p);
                break;
            case STATE_FROZEN:
                enforce_freeze(p);
                break;
            default:
                // Normal – no action needed
                break;
        }

    }
    list->energy_saved += saved_this_second;
}