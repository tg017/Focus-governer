#include "dashboard.h"
#include "process.h"
#include <ncurses.h>
#include <stdio.h>
#include <string.h>

#define MAX_PROCS 4096

float tot_cpu = 0.0;

typedef struct {
    pid_t tgid;
    char name[MAX_NAME_LEN];
    float cpu;
    int threads;
    int foreground;
    process_state_t state;
} proc_group_t;

static int quit_flag = 0;

int aggregate_processes(process_list_t *list, proc_group_t groups[]) {
    int gcount = 0;

    for (int i = 0; i < list->count; i++) {
        process_t *p = &list->processes[i];

        int found = -1;
        for (int j = 0; j < gcount; j++) {
            if (groups[j].tgid == p->tgid) {
                found = j;
                break;
            }
        }

        if (found == -1) {
            groups[gcount].tgid = p->tgid;
            strncpy(groups[gcount].name, p->name, MAX_NAME_LEN);
            groups[gcount].cpu = p->cpu_usage;
            groups[gcount].threads = 1;
            groups[gcount].foreground = p->foreground;
            groups[gcount].state = p->state;
            gcount++;
        } else {
            groups[found].cpu += p->cpu_usage;
            groups[found].threads++;
        }
    }
    for(int i=0; i<gcount; i++){
        if(groups[i].cpu > 100){
            groups[i].cpu = 100;
        }
        tot_cpu += groups[i].cpu;
    }

    return gcount;
}

void dashboard_init() {
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    timeout(1000);

    start_color();
    init_pair(1, COLOR_RED, COLOR_BLACK);    // throttled
    init_pair(2, COLOR_BLUE, COLOR_BLACK);   // frozen
    init_pair(3, COLOR_GREEN, COLOR_BLACK);  // foreground
    init_pair(4, COLOR_MAGENTA, COLOR_BLACK);  // hard throttled
}

//right panel sections
void draw_section(int col, int start_row, const char *title,
                  proc_group_t groups[], int gcount,
                  process_state_t filter_state, int only_fg) {

    attron(A_BOLD);
    mvprintw(start_row, col, "%s", title);
    attroff(A_BOLD);

    int row = start_row + 1;

    for (int i = 0; i < gcount && row < LINES - 2; i++) {
        proc_group_t *g = &groups[i];

        if (only_fg && !g->foreground) continue;
        if (!only_fg && filter_state != -1 && g->state != filter_state) continue;

        if (g->foreground)
            attron(COLOR_PAIR(3));
        else if (g->state == STATE_FROZEN)
            attron(COLOR_PAIR(2));
        else if (g->state == STATE_THROTTLED)
            attron(COLOR_PAIR(1));
        else if (g->state == STATE_HARD_THROTTLED)
            attron(COLOR_PAIR(4));

        mvprintw(row++, col, "%-8d %-40s %8.1f %8dT",
                 g->tgid, g->name, g->cpu, g->threads);

        attroff(COLOR_PAIR(1) | COLOR_PAIR(2) | COLOR_PAIR(3));
    }
}

void dashboard_update(process_list_t *list) {
    clear();

    proc_group_t groups[MAX_PROCS];
    tot_cpu = 0.0;
    int gcount = aggregate_processes(list, groups);

    int mid = COLS * 0.6; 

    attron(A_BOLD);
    mvprintw(0, 2, "FOCUS-AWARE PROCESS GOVERNOR");
    attroff(A_BOLD);

    mvprintw(1, 2, "Energy saved: %.2f CPU-sec", list->energy_saved);
    mvprintw(2, 2, "System stress: %s", list->system_stress ? "YES" : "NO");
    mvprintw(3, 2, "Average CPU Usage: %.2f | Instantaneous CPU Usage: %.2f", list->avg_cpu, tot_cpu);

    mvprintw(4, 2, "%-8s %-45s %8s %8s %-12s",
             "PID", "NAME", "CPU%", "THREADS", "STATE");
    mvprintw(5, 2, "-------------------------------------------------------------");

    int row = 6;

    for (int i = 0; i < gcount && row < LINES - 2; i++) {
        proc_group_t *g = &groups[i];

        if (g->cpu < 1.0 && !g->foreground)
            continue;

        if (g->foreground)
            attron(COLOR_PAIR(3));
        else if (g->state == STATE_FROZEN)
            attron(COLOR_PAIR(2));
        else if (g->state >= STATE_THROTTLED)
            attron(COLOR_PAIR(1));

        const char *state_str =
            (g->state == STATE_NORMAL) ? "NORM" :
            (g->state == STATE_THROTTLED) ? "THR" :
            (g->state == STATE_HARD_THROTTLED) ? "HARD" :
            (g->state == STATE_FROZEN) ? "FROZ" : "?";

        mvprintw(row++, 2, "%-8d %-45s %8.1f %8d %-12s",
                 g->tgid, g->name, g->cpu, g->threads, state_str);

        attroff(COLOR_PAIR(1) | COLOR_PAIR(2) | COLOR_PAIR(3));
    }


    draw_section(mid, 4, "[FOREGROUND]", groups, gcount, -1, 1);
    draw_section(mid, 10, "[THROTTLED]", groups, gcount, STATE_THROTTLED, 0);
    draw_section(mid, 16, "[HARD THROTTLED]", groups, gcount, STATE_HARD_THROTTLED, 0);
    draw_section(mid, 22, "[FROZEN]", groups, gcount, STATE_FROZEN, 0);

    mvprintw(LINES - 2, 2, "Press 'q' to quit");
    refresh();

    int ch = getch();
    if (ch == 'q') quit_flag = 1;
}

void dashboard_close() {
    endwin();
}

int dashboard_should_quit() {
    return quit_flag;
}