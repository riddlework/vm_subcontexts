#include <stdio.h>
#include <stdlib.h>
#include "vm_sbc.h"

void function1(int arg) {
    int a = 2, b = 3;
    printf("%d + %d = %d\n", a, b, a + b);
}

void function2(int arg) {
    int a = 4, b = 5;
    printf("%d * %d = %d\n", a, b, a * b);
}

int main(void) {
    void (*funcs[2])(int) = { function1, function2 };

    // print address of functions for debugging
    printf("Function addresses:\n");
    printf("function1: %p\n", (void*)function1);
    printf("function2: %p\n", (void*)function2);

    if (create_image_file(__FILE_NAME__, funcs, 2) != EXIT_SUCCESS) {
        fprintf(stderr, "Failed to create image file\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
