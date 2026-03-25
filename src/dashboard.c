#include "dashboard.h"
#include "process.h"
#include <ncurses.h>
#include <stdio.h>
#include <string.h>

static int quit_flag = 0;

void dashboard_init() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    timeout(1000);           // getch() waits up to 1 second, then returns ERR
    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);   // throttled
    init_pair(2, COLOR_BLUE, COLOR_BLACK);  // frozen
    init_pair(3, COLOR_GREEN, COLOR_BLACK); // foreground
}

void dashboard_update(process_list_t *list) {
    clear();

    // Title
    attron(A_BOLD);
    mvprintw(0, 2, "FOCUS-AWARE PROCESS GOVERNOR");
    attroff(A_BOLD);
    mvprintw(1, 2, "Energy saved: %.2f CPU-seconds", list->energy_saved);
    mvprintw(2, 2, "System stress: %s", list->system_stress ? "YES" : "NO");

    // Header
    mvprintw(4, 2, "%-8s %-8s %-45s %8s %4s %-12s",
             "PID", "TGID", "NAME", "CPU%", "FG", "STATE");
    mvprintw(5, 2, "------------------------------------------------------------------");

    int row = 6;
    int max_rows = LINES - 4;
    for (int i = 0; i < list->count && row < max_rows; i++) {
        process_t *p = &list->processes[i];

        if((int)p->cpu_usage<1 && (!p->foreground || p->state != STATE_NORMAL)){
            continue;
        }

        // Color based on state
        if (p->state == STATE_FROZEN)
            attron(COLOR_PAIR(2));
        else if (p->state >= STATE_THROTTLED)
            attron(COLOR_PAIR(1));
        else if (p->foreground)
            attron(COLOR_PAIR(3));

        const char *state_str;
        switch (p->state) {
            case STATE_NORMAL: state_str = "NORM"; break;
            case STATE_THROTTLED: state_str = "THR"; break;
            case STATE_HARD_THROTTLED: state_str = "HARD"; break;
            case STATE_FROZEN: state_str = "FROZ"; break;
            default: state_str = "?";
        }

        mvprintw(row++, 2, "%-8d %-8d %-45s %8.1f %4d %-12s",
                 p->pid, p->tgid, p->name, p->cpu_usage, p->foreground, state_str);

        attroff(COLOR_PAIR(1) | COLOR_PAIR(2) | COLOR_PAIR(3));
    }

    mvprintw(LINES-2, 2, "Press 'q' to quit");
    refresh();

    // Check for key press
    int ch = getch();
    if (ch == 'q') {
        quit_flag = 1;
    }
}

void dashboard_close() {
    endwin();
}

int dashboard_should_quit() {
    return quit_flag;
}