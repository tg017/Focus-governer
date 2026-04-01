#ifndef MONITOR_H
#define MONITOR_H

#include "process.h"

long get_clock_ticks();

int scan_processes(process_list_t *list);

int read_process_stats(pid_t pid,pid_t tid, unsigned long *utime, 
                       unsigned long *stime, char *name, int name_len);

void update_cpu_usage(process_list_t *list);

#endif