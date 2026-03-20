#ifndef ENFORCE_H
#define ENFORCE_H

#include "process.h"

// Safety check: skip kernel/system processes
int is_safe_to_throttle(pid_t pid);

// Level 1: set nice value to +19
void enforce_nice(process_t *proc);

// Level 2: move process to a cgroup with CPU cap
void enforce_cgroup(process_t *proc);

// Level 3: send SIGSTOP to freeze process
void enforce_freeze(process_t *proc);

// Wake a frozen process (SIGCONT)
void enforce_wake(process_t *proc);

// Clean up cgroup when process exits
void cleanup_cgroup(process_t *proc);

// Called from main loop to apply all enforcement
void apply_enforcement(process_list_t *list);

#endif