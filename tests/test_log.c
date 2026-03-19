#include "src/utils.h"
#include <stdio.h>

int main() {
    printf("Testing logging...\n");
    
    log_message("INFO", "This is a test message");
    log_action("TEST", 1234, "firefox");
    
    printf("Check logs/governor.log for entries\n");
    
    return 0;
}