#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "vm_sbc.h"

int main(int argc, char **argv) {
    printf("Starting subcontext mapping test\n");
    
    // map the test image file
    int fd = map_subcontext("img_files/test.img");
    if (fd < 0) {
        printf("Failed to map subcontext image\n");
        return EXIT_FAILURE;
    }
    
    printf("Successfully mapped test.img subcontext\n");
    printf("Now attempting to call dummy_function1 (index 0)\n");
    
    // call the first dummy function from the subcontext
    int result = call_subcontext_function(0, fd);
    if (result != EXIT_SUCCESS) {
        printf("Failed to call function from subcontext\n");
        close(fd);
        return EXIT_FAILURE;
    }

    // call the first dummy function from the subcontext
    result = call_subcontext_function(1, fd);
    if (result != EXIT_SUCCESS) {
        printf("Failed to call function from subcontext\n");
        close(fd);
        return EXIT_FAILURE;
    }
    
    // call the first dummy function from the subcontext
    result = call_subcontext_function(1, fd);
    if (result != EXIT_SUCCESS) {
        printf("Failed to call function from subcontext\n");
        close(fd);
        return EXIT_FAILURE;
    }
    
    printf("Successfully called function from subcontext\n");
    
    printf("Successfully called function from subcontext\n");
    
    printf("Successfully called function from subcontext\n");
    
    // clean up
    close(fd);
    return EXIT_SUCCESS;
}
