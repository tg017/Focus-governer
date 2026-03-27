#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h>
#include <time.h>

#define MAX_NAME_LEN 256
#define HISTORY_SIZE 3

typedef enum {
    STATE_NORMAL = 0,
    STATE_THROTTLED,
    STATE_HARD_THROTTLED,
    STATE_FROZEN
} process_state_t;


typedef struct {
    pid_t pid;
    pid_t tgid;   // Thread Group ID (i.e., actual process ID)
    char name[MAX_NAME_LEN];
    unsigned long window_id;
    
    // CPU tracking
    float cpu_usage;
    unsigned long last_utime;
    unsigned long last_stime;
    float history[HISTORY_SIZE];
    int history_index;
    
    // Process state
    int foreground;        // 1 if foreground, 0 if background
    process_state_t state;
    int violations;
    
    // Timestamps
    time_t first_seen;
    time_t last_seen;
    
} process_t;

typedef struct {
    process_t *processes;
    int count;
    int capacity;
    float energy_saved;

    int system_stress;
} process_list_t;

// Function declarations
process_list_t* init_process_list();
void free_process_list(process_list_t *list);
void add_process(process_list_t *list, pid_t pid, const char *name);
void remove_process(process_list_t *list, int index);
process_t* find_process(process_list_t *list, pid_t pid);
void print_process_list(process_list_t *list);

#endif