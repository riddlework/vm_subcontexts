#include <stdio.h>
#include <stdlib.h>
#include "vm_sbc.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <img_file> [img_file...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    init();

    int fds[MAX_IMG_FILES];
    size_t num_fds = 0;

    for (int i = 1; i < argc && num_fds < MAX_IMG_FILES; i++) {
        const char *img = argv[i];
        printf("Mapping image: %s\n", img);
        int fd = map_subcontext(img);
        if (fd < 0) {
            fprintf(stderr, "Failed to map %s\n", img);
            continue;
        }
        fds[num_fds++] = fd;

        int idx = 0;
        while (call_subcontext_function(idx, fd) == EXIT_SUCCESS) {
            idx++;
        }
        printf("Executed %d functions from %s\n", idx, img);
    }

    finalize();
    return EXIT_SUCCESS;
}
