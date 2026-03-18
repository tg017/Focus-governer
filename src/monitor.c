#include "monitor.h"
#include "process.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <ctype.h>

long get_clock_ticks() {
    long ticks = sysconf(_SC_CLK_TCK);
    if (ticks == -1) {
        perror("sysconf");
        return 100;  // Default fallback
    }
    return ticks;
}

int is_numeric(const char *str) {
    while (*str) {
        if (!isdigit(*str)) return 0;
        str++;
    }
    return 1;
}

int read_process_stats(pid_t pid,pid_t tid, unsigned long *utime, 
                       unsigned long *stime, char *name, int name_len) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/task/%d/stat", pid, tid);
    
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    char line[1024];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    
    // Find the process name in parentheses
    char *open_paren = strchr(line, '(');
    char *close_paren = strrchr(line, ')');
    
    if (!open_paren || !close_paren) return -1;
    
    // Extract name
    int name_len_actual = close_paren - open_paren - 1;
    if (name_len_actual > name_len - 1) name_len_actual = name_len - 1;
    strncpy(name, open_paren + 1, name_len_actual);
    name[name_len_actual] = '\0';
    
    // Parse fields after the closing parenthesis
    char *p = close_paren + 2;

    // Skip state field
    while (*p && *p != ' ')
        p++;
    p++;

    int field = 3;  // because we already skipped fields 1 and 2

    while (*p) {
        char *end;
        unsigned long val = strtoul(p, &end, 10);
        if (p == end) break;

        if (field == 14) *utime = val;
        if (field == 15) *stime = val;

        p = end;
        field++;
    }
    
    return 0;
}

int scan_processes(process_list_t *list) {
    DIR *proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("opendir /proc");
        return -1;
    }
    
    struct dirent *entry;
    int scanned = 0;
    
    // First, mark all existing processes as not seen
    for (int i = 0; i < list->count; i++) {
        list->processes[i].last_seen = 0;  // Will be updated if seen
    }

    while ((entry = readdir(proc_dir)) != NULL) {
        if (!is_numeric(entry->d_name)) continue;

        pid_t pid = atoi(entry->d_name);

        // Open task directory
        char task_path[256];
        snprintf(task_path, sizeof(task_path), "/proc/%d/task", pid);

        DIR *task_dir = opendir(task_path);
        if (!task_dir) continue;

        struct dirent *task_entry;

        while ((task_entry = readdir(task_dir)) != NULL) {
            if (!is_numeric(task_entry->d_name)) continue;

            pid_t tid = atoi(task_entry->d_name);

            unsigned long utime, stime;
            char name[MAX_NAME_LEN];

            if (read_process_stats(pid, tid, &utime, &stime, name, sizeof(name)) == 0) {

                process_t *proc = find_process(list, tid);

                if (proc) {
                    // Existing thread
                    proc->last_utime = utime;
                    proc->last_stime = stime;
                    proc->last_seen = time(NULL);
                } else {
                    // New thread
                    add_process(list, tid, name);
                    proc = find_process(list, tid);

                    if (proc) {
                        proc->last_utime = utime;
                        proc->last_stime = stime;
                        proc->last_seen = time(NULL);
                    }
                }
            }
        }

        closedir(task_dir);

    }
    
    // Remove processes that weren't seen in this scan
    for (int i = list->count - 1; i >= 0; i--) {
        if (list->processes[i].last_seen == 0) {
            remove_process(list, i);
        }
    }
    
    return scanned;
}

void update_cpu_usage(process_list_t *list) {
    static int first_run = 1;
    static unsigned long *prev_utime = NULL;
    static unsigned long *prev_stime = NULL;
    static pid_t *prev_pids = NULL;
    static int prev_count = 0;
    
    long hz = get_clock_ticks();
    
    if (first_run) {
        // First run: just store current values
        prev_count = list->count;
        prev_utime = malloc(prev_count * sizeof(unsigned long));
        prev_stime = malloc(prev_count * sizeof(unsigned long));
        prev_pids = malloc(prev_count * sizeof(pid_t));
        
        for (int i = 0; i < list->count; i++) {
            prev_utime[i] = list->processes[i].last_utime;
            prev_stime[i] = list->processes[i].last_stime;
            prev_pids[i] = list->processes[i].pid;
        }
        first_run = 0;
        return;
    }
    
    // Second and subsequent runs: calculate CPU usage
    for (int i = 0; i < list->count; i++) {
        process_t *p = &list->processes[i];
        
        // Find matching previous entry by PID
        for (int j = 0; j < prev_count; j++) {
            if (p->pid == prev_pids[j]) {
                unsigned long utime_diff = p->last_utime - prev_utime[j];
                unsigned long stime_diff = p->last_stime - prev_stime[j];
                unsigned long total_jiffies = utime_diff + stime_diff;
                
                // CPU% over 1 second interval
                p->cpu_usage = (total_jiffies * 100.0) / hz;
                if (p->cpu_usage > 100.0) p->cpu_usage = 100.0;
                
                // Update history
                p->history[p->history_index % HISTORY_SIZE] = p->cpu_usage;
                p->history_index++;
                break;
            }
        }
    }
    
    // Update previous values for next iteration
    free(prev_utime);
    free(prev_stime);
    free(prev_pids);
    
    prev_count = list->count;
    prev_utime = malloc(prev_count * sizeof(unsigned long));
    prev_stime = malloc(prev_count * sizeof(unsigned long));
    prev_pids = malloc(prev_count * sizeof(pid_t));
    
    for (int i = 0; i < list->count; i++) {
        prev_utime[i] = list->processes[i].last_utime;
        prev_stime[i] = list->processes[i].last_stime;
        prev_pids[i] = list->processes[i].pid;
    }
}