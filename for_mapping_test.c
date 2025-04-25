#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "vm_sbc.h"

// Forward declaration for map_subcontext
int map_subcontext(const char *image_file);

// Forward declaration of call_subcontext_function
int call_subcontext_function(int func_index, int fd);

int main(int argc, char **argv) {
    printf("Starting subcontext mapping test\n");
    
    // Map the test image file
    int fd = map_subcontext("test.img");
    if (fd < 0) {
        printf("Failed to map subcontext image\n");
        return EXIT_FAILURE;
    }
    
    printf("Successfully mapped test.img subcontext\n");
    printf("Now attempting to call dummy_function1 (index 0)\n");
    
    // Call the first dummy function from the subcontext
    int result = call_subcontext_function(0, fd);
    if (result != EXIT_SUCCESS) {
        printf("Failed to call function from subcontext\n");
        close(fd);
        return EXIT_FAILURE;
    }
    
    printf("Successfully called function from subcontext\n");
    
    // Clean up
    close(fd);
    return EXIT_SUCCESS;
}
