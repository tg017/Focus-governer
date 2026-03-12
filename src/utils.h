#ifndef UTILS_H
#define UTILS_H

#include "process.h"

void log_message(const char *level, const char *message);
void log_action(const char *action, pid_t pid, const char *name);
void print_banner();
void print_stats(process_list_t *list);

#endif