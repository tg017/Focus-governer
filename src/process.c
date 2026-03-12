#include "process.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

process_list_t* init_process_list() {
    process_list_t *list = malloc(sizeof(process_list_t));
    if (!list) return NULL;
    
    list->processes = NULL;
    list->count = 0;
    list->capacity = 0;
    return list;
}

void free_process_list(process_list_t *list) {
    if (list) {
        free(list->processes);
        free(list);
    }
}

void add_process(process_list_t *list, pid_t pid, const char *name) {
    // Check if already exists
    if (find_process(list, pid)) return;
    
    // Expand if needed
    if (list->count >= list->capacity) {
        list->capacity = list->capacity == 0 ? 10 : list->capacity * 2;
        process_t *new_procs = realloc(list->processes, 
                                  list->capacity * sizeof(process_t));
        if (!new_procs) {
            fprintf(stderr, "Failed to allocate memory\n");
            return;
        }
        list->processes = new_procs;
    }
    
    // Add new process
    process_t *p = &list->processes[list->count++];
    p->pid = pid;
    strncpy(p->name, name, MAX_NAME_LEN - 1);
    p->name[MAX_NAME_LEN - 1] = '\0';
    
    p->cpu_usage = 0.0;
    p->last_utime = 0;
    p->last_stime = 0;
    p->foreground = 0;
    p->state = 0;
    
    for (int i = 0; i < HISTORY_SIZE; i++) {
        p->history[i] = 0.0;
    }
    p->history_index = 0;
    
    p->first_seen = time(NULL);
    p->last_seen = p->first_seen;
}

void remove_process(process_list_t *list, int index) {
    if (index < 0 || index >= list->count) return;
    
    // Shift all elements left
    for (int i = index; i < list->count - 1; i++) {
        list->processes[i] = list->processes[i + 1];
    }
    list->count--;
}

process_t* find_process(process_list_t *list, pid_t pid) {
    for (int i = 0; i < list->count; i++) {
        if (list->processes[i].pid == pid) {
            return &list->processes[i];
        }
    }
    return NULL;
}

void print_process_list(process_list_t *list) {
    printf("\n%-8s %-20s %-8s\n", "PID", "NAME", "CPU%");
    printf("----------------------------------------\n");
    
    for (int i = 0; i < list->count; i++) {
        printf("%-8d %-20s %-8.1f\n", 
               list->processes[i].pid,
               list->processes[i].name,
               list->processes[i].cpu_usage);
    }
    printf("\n");
}