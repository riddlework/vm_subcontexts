#include <stdio.h>
#include <stdlib.h>
#include "vm_sbc.h"

/*
 * maps the matchmaker into the client process' memory
 */
void init() {

}


/*
 * ask the matchmaker to map a server image of the given
 * file name into its memory
 */
int request_map(const char *img_fname) {
    return 0;

}

/*
 * ask the matchmaker to call the function with the given
 * index from the given server image
 */
int request_func_call(int func_idx, int fd) {
    // map just the header to get the function pointer
    Header *header;
    void *header_map = mmap(NULL, sizeof(Header), PROT_READ, MAP_PRIVATE, fd, 0);
    
    if (header_map == MAP_FAILED) {
        perror("Error mapping header for function call");
        return EXIT_FAILURE;
    }
    
    header = (Header *)header_map;
    
    // check if the function index is valid
    if (func_idx < 0 || func_idx >= MAX_FUNC_PTRS || header->func_ptr[func_idx] == NULL) {
        fprintf(stderr, "Invalid function index or NULL function pointer\n");
        munmap(header_map, sizeof(Header));
        return EXIT_FAILURE;
    }
    
    // Get the function pointer
    void (*func)(int) = header->func_ptr[func_idx];
    
    printf("Calling function at address: %p\n", func);
    
    // Call the function
    func(0);
    
    // Clean up
    munmap(header_map, sizeof(Header));
    
    return EXIT_SUCCESS;

}


/* 
 * unmap all mapped server images from the client
 * process' memory. do this and then unmap self.
 * should be called upon finalization of the client program
 */
void finalize() {

}

