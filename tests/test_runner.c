
/**
 * Test Runner
 */

#include <stdio.h>

// Declare test functions
void run_database_tests(void);
void run_utils_tests(void);
void run_index_tests(void);
void run_parser_tests(void);

int main() {
    printf("Starting Luminix Test Suite\n");
    printf("===========================\n\n");
    
    run_database_tests();
    printf("\n");
    
    run_utils_tests();
    printf("\n");
    
    run_index_tests();
    printf("\n");
    
    run_parser_tests();
    printf("\n");
    
    printf("All tests completed.\n");
    return 0;
}
