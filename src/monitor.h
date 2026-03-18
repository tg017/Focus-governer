#ifndef MONITOR_H
#define MONITOR_H

#include "process.h"

// Get system clock ticks per second (usually 100)
long get_clock_ticks();

// Scan /proc and update process list
int scan_processes(process_list_t *list);

// Read /proc/[pid]/stat and extract utime/stime
int read_process_stats(pid_t pid,pid_t tid, unsigned long *utime, 
                       unsigned long *stime, char *name, int name_len);

// Update CPU usage for all processes
void update_cpu_usage(process_list_t *list);

#endif