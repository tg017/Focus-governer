#ifndef ENFORCE_H
#define ENFORCE_H

#include "process.h"

int is_safe_to_throttle(pid_t pid);

void enforce_nice(process_t *proc);

void enforce_cgroup(process_t *proc);

void enforce_freeze(process_t *proc);

void enforce_wake(process_t *proc);

void cleanup_cgroup(process_t *proc);

void apply_enforcement(process_list_t *list);

#endif