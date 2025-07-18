#include <stdio.h>
#include <stdlib.h>
#include "vm_sbc.h"

void function1(int arg) {
    printf("Goodbye from test3!\n");
}

void function2(int arg) {
    int a = 7, b = 4;
    printf("%d - %d = %d\n", a, b, a - b);
}

void function3(int arg) {
    printf("Greetings from test3! arg=%d\n", arg);
}

int main(void) {
    void (*funcs[3])(int) = { function1, function2, function3 };

    // print address of functions for debugging
    printf("Function addresses:\n");
    printf("function1: %p\n", (void*)function1);
    printf("function2: %p\n", (void*)function2);
    printf("function3: %p\n", (void*)function3);

    if (create_image_file(__FILE_NAME__, funcs, 3) != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to create image file\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
