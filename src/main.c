#include "process.h"
#include "monitor.h"
#include "utils.h"
#include "policy.h"
#include "enforce.h"
#include "dashboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

volatile int running = 1;
pid_t GOVERNOR_PID;

void handle_signal(int sig) {
    if (sig == SIGINT) {
        printf("\nShutting down...\n");
        running = 0;
    }
}

int main() {

    GOVERNOR_PID = getpid();

    // Set up signal handler for Ctrl+C
    signal(SIGINT, handle_signal);
    
    // Initialize
    print_banner();
    log_message("INFO", "Governor started (Phase 1)");
    
    process_list_t *processes = init_process_list();
    if (!processes) {
        fprintf(stderr, "Failed to initialize process list\n");
        return 1;
    }
    
    long hz = get_clock_ticks();
    printf("System clock ticks per second: %ld\n", hz);
    printf("Monitoring every 1 second. Press Ctrl+C to stop.\n");
    printf("Starting ncurses dashboard...\n");
    dashboard_init();   // start ncurses mode
    
    // Main loop
    int first_scan = 1;
    int scan_count = 0;
    
    while (running) {
        // Scan /proc
        int count = scan_processes(processes);
        
        if (first_scan) {
            // First scan: just collect baseline
            first_scan = 0;
            log_message("INFO", "First scan complete - collecting baseline");
            // printf("First scan complete. Found %d processes.\n", processes->count);
        } else {
            // Calculate CPU usage for all processes
            update_cpu_usage(processes);

            //Marking Foreground
            pid_t fg_pid = get_foreground_pid();

            update_foreground_status(processes, fg_pid);

            apply_policy(processes);

            apply_enforcement(processes);

            dashboard_update(processes);
            
            // Display stats every 5 iterations to avoid clutter
            // Temporary for every 2 iterations
            // if (scan_count % 1 == 0) {
            //     print_stats(processes);
            // }
            
            // Log total processes periodically
            if (scan_count % 10 == 0) {
                char msg[100];
                snprintf(msg, sizeof(msg), "Active processes: %d", processes->count);
                log_message("INFO", msg);
            }
            
            scan_count++;
        }

        if (dashboard_should_quit()) {
            running = 0;
            break;
        }
        
        usleep(1000000);
    }
    
    // Cleanup
    dashboard_close();
    free_process_list(processes);
    log_message("INFO", "Governor stopped");
    printf("Cleanup complete. Exiting.\n");
    
    return 0;
}