#include "utils.h"
#include "process.h"
#include <stdio.h>
#include<stdlib.h>
#include <time.h>

void log_message(const char *level, const char *message) {
    FILE *log = fopen("logs/governor.log", "a");
    if (!log) return;
    
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(log, "[%s] %s: %s\n", timestamp, level, message);
    fclose(log);
}

void log_action(const char *action, pid_t pid, const char *name) {
    FILE *log = fopen("logs/governor.log", "a");
    if (!log) return;
    
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    fprintf(log, "[%s] ACTION %s: PID %d (%s)\n", timestamp, action, pid, name);
    fclose(log);
}

void print_banner() {
    printf("\n");
    printf("========================================\n");
    printf("  Focus-Aware Process Governor  \n");
    printf("========================================\n");
    printf("\n");
}

// void print_stats(process_list_t *list) {
//     static int counter = 0;
//     printf("\n[Update %d] ", ++counter);
//     fflush(stdout);
//     system("date");
//     print_process_list(list);
// }