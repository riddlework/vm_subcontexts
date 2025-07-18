#include <stdio.h>
#include <stdlib.h>
#include "vm_sbc.h"

void function1(int arg) {
    printf("Hello from test1! arg=%d\n", arg);
}

int main(void) {
    void (*funcs[1])(int) = { function1 };

    // print address of functions for debugging
    printf("Function addresses:\n");
    printf("function1: %p\n", (void*)function1);

    if (create_image_file(__FILE_NAME__, funcs, 1) != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to create image file\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
