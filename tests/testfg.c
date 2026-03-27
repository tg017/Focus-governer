#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

// Get active window ID
unsigned long get_active_window() {
    FILE *fp = popen("xdotool getwindowfocus 2>/dev/null", "r");
    if (!fp) return 0;

    unsigned long win = 0;
    fscanf(fp, "%lu", &win);
    pclose(fp);
    return win;
}

// Get PID from window ID
pid_t get_pid_from_window(unsigned long win) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "xdotool getwindowpid %lu 2>/dev/null", win);

    FILE *fp = popen(cmd, "r");
    if (!fp) return -1;

    pid_t pid = -1;
    fscanf(fp, "%d", &pid);
    pclose(fp);
    return pid;
}

int main() {

    printf("=== Window Freeze/Wake Test ===\n");
    printf("Step 1: Focus a window (e.g., Firefox/Terminal)\n");
    printf("Waiting 3 seconds...\n\n");
    sleep(3);

    // Step 1: Capture window + PID
    unsigned long win = get_active_window();
    pid_t pid = get_pid_from_window(win);

    if (win == 0 || pid <= 0) {
        printf("Failed to get window or PID\n");
        return 1;
    }

    printf("Captured:\n");
    printf("Window ID: %lu\n", win);
    printf("PID: %d\n\n", pid);

    // Step 2: Freeze process
    printf("Freezing process...\n");
    kill(pid, SIGSTOP);

    printf("Process frozen.\n");
    printf("Now click the SAME window to wake it.\n\n");

    sleep(10);

    // Step 3: Wait for user to refocus
    while (1) {
        unsigned long active_win = get_active_window();

        if (active_win == win) {
            printf("Window focused again! Waking process...\n");
            kill(pid, SIGCONT);
            break;
        }

        usleep(200000); // 200 ms
    }

    printf("Process resumed successfully.\n");

    return 0;
}