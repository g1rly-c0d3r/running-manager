#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../server/rund.c"

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

// if you add tests, you will need to update this number
#define NUM_TESTS 1

// Declare tests here
void test_remove_node(void);


void run_tests(char** tests, int numtest);

// If you add tests, you will need to add them to this function pointer array
void (*test_fns[NUM_TESTS])(void) = {
    test_remove_node,
};

// tests if argv has a valid list of tests, 
// or if we want to run all of the tests
bool valid(char **tests, int numtest){
    if(strcmp(tests[1], "all") == 0){
        return true;
    }

    for(int i = 1; i < numtest; i++){
        if (strtoul(tests[i], NULL, 10) == 0 || strtoul(tests[i], NULL, 10) > NUM_TESTS) {
            fprintf(stderr, "Tests range from 1-%d\n", NUM_TESTS);
            return false;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2 || !valid(argv, argc) ){
        fprintf(stderr, "Usage: ./test < all | test_num_list >\n");
        exit(1);
    }
    run_tests(argv, argc);

    return 0;
}


void run_tests(char **tests, int numtest){
    if (strcmp(tests[1],  "all") == 0){
        for (uint8_t i = 0; i < NUM_TESTS; i++){
            test_fns[i]();
        }
    } else {
        for(uint8_t i = 1; i < numtest; i++){
            test_fns[strtoul(tests[i],NULL, 10)-1]();
        }
    }
}

void test_remove_node(void){
}
