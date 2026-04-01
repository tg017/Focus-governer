#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "process.h"

void dashboard_init();
void dashboard_update(process_list_t *list);
void dashboard_close();
int dashboard_should_quit();

#endif