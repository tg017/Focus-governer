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

// Skip kernel threads (pid < 100) and essential system processes
int is_safe_to_throttle(pid_t pid) {
    if (pid < 100) return 0;  // kernel threads
    if (pid == GOVERNOR_PID) return 0;

    // Optionally read /proc/[pid]/cmdline to filter known system daemons
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    FILE *f = fopen(path, "r");
    if (f) {
        char cmdline[256];
        size_t len = fread(cmdline, 1, sizeof(cmdline)-1, f);
        fclose(f);
        cmdline[len] = '\0';
        // If cmdline is empty, it's likely a kernel thread
        if (len == 0) return 0;
        // Add more checks for systemd, init, etc. if desired
    }
    return 1;
}

void boost_foreground(process_t *proc) {
    if (!proc) return;

    if (setpriority(PRIO_PROCESS, proc->tgid, -10) == 0) {
        // log_action("BOOST", proc->pid, proc->name);
    }
}

// Level 1: set nice value to 19
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

// Placeholder for other functions (to be filled later)
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
    if (proc->state != STATE_FROZEN) return; // already frozen?

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

    // 🔹 Move process back to root cgroup
    FILE *f = fopen("/sys/fs/cgroup/cgroup.procs", "w");
    if (f) {
        fprintf(f, "%d", proc->tgid);
        fclose(f);
    }

    // 🔹 Remove cgroup directory
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

    // Find foreground main thread
    for (int i = 0; i < list->count; i++) {
        process_t *p = &list->processes[i];

        if (p->pid == p->tgid && p->foreground) {
            fg_proc = p;
            break;
        }
    }

    for (int i = 0; i < list->count; i++) {
        process_t *p = &list->processes[i];


        // BOOST ONLY WHEN SYSTEM IS UNDER STRESS
        if (fg_proc && list->system_stress) {
            boost_foreground(fg_proc);
        }


        // Only enforce on one thread per process to avoid duplicates
        // We'll enforce on the thread that is also the tgid (main thread)
        if (p->pid != p->tgid) continue;   // only main thread does enforcement
        if (p->tgid == GOVERNOR_PID) continue;

        if (p->state == STATE_THROTTLED || p->state == STATE_HARD_THROTTLED) {
            // The process is being limited; we assume it would have used up to threshold
            // float prevented = p->cpu_usage / 100.0;
            // if (prevented > 0 && list->system_stress) saved_this_second += prevented;
        }

        // unsigned long active_win = get_active_window();
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

        // If process is foreground and frozen, wake it
        // if (p->foreground && p->state == STATE_FROZEN) {
        //     // printf("%d, tried to wake _______________________________________\n", p->pid);
        //     enforce_wake(p);
        // }
    }
    list->energy_saved += saved_this_second;
}