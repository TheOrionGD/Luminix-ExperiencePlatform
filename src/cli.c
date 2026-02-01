/**
 * Command Line Interface for Luminix Database
 */

#include "cli.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void cli_init(void) {
    printf("Luminix CLI Initialized\n");
}

void cli_handle_command(const char* command) {
    // TODO: Implement command handling
    printf("Command received: %s\n", command);
}

void cli_run(void) {
    char input[256];
    
    printf("Luminix Database CLI (Type 'exit' to quit)\n");
    
    while (1) {
        printf("> ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Remove newline
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            break;
        }
        
        cli_handle_command(input);
    }
}
