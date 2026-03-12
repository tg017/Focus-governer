#include "src/process.h"
#include <stdio.h>

int main() {
    process_list_t *list = init_process_list();
    
    printf("Adding processes...\n");
    add_process(list, 1234, "firefox");
    add_process(list, 5678, "bash");
    add_process(list, 9012, "code");
    
    printf("Process list should have 3 entries:\n");
    print_process_list(list);
    
    printf("Finding PID 5678...\n");
    process_t *p = find_process(list, 5678);
    if (p) {
        printf("Found: %d - %s\n", p->pid, p->name);
    }
    
    printf("Removing PID 1234...\n");
    remove_process(list, 0); // removes first element
    
    printf("Process list should now have 2 entries:\n");
    print_process_list(list);
    
    free_process_list(list);
    return 0;
}