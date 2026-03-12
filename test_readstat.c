#include "src/monitor.h"
#include <stdio.h>

int main() {
    unsigned long utime, stime; 
    char name[256];
    
    // Test reading current process's stats
    pid_t my_pid = getpid();
    
    printf("Reading stats for PID %d (this program)...\n", my_pid);
    int result = read_process_stats(my_pid, &utime, &stime, name, sizeof(name));
    
    if (result == 0) {
        printf("SUCCESS!\n");
        printf("Process name: %s\n", name);
        printf("utime: %lu\n", utime);
        printf("stime: %lu\n", stime);
        printf("Total jiffies: %lu\n", utime + stime);
    } else {
        printf("FAILED to read stats\n");
    }
    
    return 0;
}