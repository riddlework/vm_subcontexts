#include <stdio.h>
#include <stdlib.h>
#include "vm_sbc.h"

// dummy function definitions
void dummy_function1(int arg) {
    printf("This is dummy function 1 with argument: %d\n", arg);
}

void dummy_function2(int arg) {
    printf("This is dummy function 2 with argument: %d\n", arg);
}

void dummy_function3(int arg) {
    printf("Hello from dummy function 3! Argument received: %d\n", arg);
}

int main(int argc, char **argv) {
    printf("Starting memory snapshot test program\n");
    /*dummy_function1(0);*/
    
    // create array of function pointers
    void (*function_array[3])(int) = {
        dummy_function1,
        dummy_function2,
        dummy_function3
    };
    
    // print addresses of our functions for debugging
    printf("Function addresses:\n");
    printf("dummy_function1: %p\n", (void*)dummy_function1);
    printf("dummy_function2: %p\n", (void*)dummy_function2);
    printf("dummy_function3: %p\n", (void*)dummy_function3);
    
    // call create_image_file with our function pointers
    int result = create_image_file("test.c", function_array, 3);
    
    // check result
    if (result == EXIT_SUCCESS) {
        printf("Memory snapshot created successfully!\n");
    } else {
        printf("Failed to create memory snapshot.\n");
    }
    
    return result;
}
