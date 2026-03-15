#ifndef POLICY_H
#define POLICY_H

#include "process.h"

pid_t get_foreground_pid();

void update_foreground_status(process_list_t *list, pid_t fg_pid);

void apply_policy(process_list_t *list);

#endif