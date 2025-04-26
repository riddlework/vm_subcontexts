#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include "vm_sbc.h"

int main(int argc, char **argv) {
    printf("Starting subcontext mapping test\n");
    
    // retrieve filename
    assert(argv[1] != NULL);
    const char *img_filename = argv[1];

    // map the test image file
    char buf[SMLBUFSZ] = {0};
    sprintf(buf, "img_files/%s", img_filename);
    int fd = map_subcontext(buf);
    if (fd < 0) {
        printf("Failed to map subcontext image\n");
        return EXIT_FAILURE;
    }
    
    printf("Successfully mapped %s's subcontext\n", img_filename);
    printf("Now attempting to call dummy_function1 (index 0)\n");
    
    // call the first dummy function from the subcontext
    int result = call_subcontext_function(0, fd);
    if (result != EXIT_SUCCESS) {
        printf("Failed to call function from subcontext\n");
        close(fd);
        return EXIT_FAILURE;
    }

    printf("Successfully called function from subcontext\n");

    // call the second dummy function from the subcontext
    result = call_subcontext_function(1, fd);
    if (result != EXIT_SUCCESS) {
        printf("Failed to call function from subcontext\n");
        close(fd);
        return EXIT_FAILURE;
    }
    
    printf("Successfully called function from subcontext\n");

    // call the third dummy function from the subcontext
    result = call_subcontext_function(1, fd);
    if (result != EXIT_SUCCESS) {
        printf("Failed to call function from subcontext\n");
        close(fd);
        return EXIT_FAILURE;
    }
    
    printf("Successfully called function from subcontext\n");
    
    // clean up
    close(fd);
    return EXIT_SUCCESS;
}
