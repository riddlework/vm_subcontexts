#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include "vm_sbc.h"


/**
 * creates a snapshot of the current program's memory and stores it in an image file.
 * The image file can later be used to recreate the memory state in another process.
 *
 * @param filename the name of the C file to be associated with the image (output will be filename.img)
 * @param func_list array of function pointers to store in the header
 * @param num_funcs number of function pointers in the array
 * @return 0 on success, non-zero on failure
 */
int create_image_file(const char *filename, void (**func_list)(int), size_t num_funcs) {

    // get a pointer to the dot
    char *dot = strrchr(filename, '.');
    if (dot == NULL) {
        perror("strchr returned NULL!\n");
        return EXIT_FAILURE;
    }
    size_t base_len;
    char *underscore = strchr(filename, '_');
    if (underscore)
        base_len = dot - underscore - 1;
    else
        base_len = dot - filename;

    char output_filename[256];
    assert(base_len < sizeof(output_filename) - 10);
    memcpy(output_filename, "img_files/", 11);
    if (underscore)
        memcpy(output_filename + 10, underscore + 1, base_len);
    else
        memcpy(output_filename + 10, filename, base_len);
    memcpy(output_filename + 10 + base_len, ".img", 5);
    
    printf("Creating memory snapshot in file: %s\n", output_filename);
    
    // Open /proc/self/maps to read current memory mappings
    int maps_fd = open("/proc/self/maps", O_RDONLY);
    if (maps_fd == -1) {
        perror("Error opening /proc/self/maps");
        return EXIT_FAILURE;
    }

    // perform read
    char buf[BUFSZ];
    ssize_t bytes_read = read(maps_fd, buf, BUFSZ - 1);
    
    // error checking for read
    if (bytes_read == -1) {
        perror("Error reading maps file");
        close(maps_fd);
        return EXIT_FAILURE;
    }
    
    // error checking for buffer
    if (bytes_read >= BUFSZ - 1) {
        fprintf(stderr, "Buffer too small for maps file\n");
        close(maps_fd);
        return EXIT_FAILURE;
    }
    
    // null terminate the string
    buf[bytes_read] = '\0';
    close(maps_fd);

    // arrays for memory region information
    ulong starts[MAX_ENTRIES];
    ulong ends[MAX_ENTRIES];
    char perms[MAX_ENTRIES][5];
    size_t num_regions = 0;

    // parse the buffer line by line
    char *line = strtok(buf, "\n");
    while (line && num_regions < MAX_ENTRIES) {

        // skip excluded regions
        if (!should_exclude_region(line)) {
            ulong start, end;
            char perm_buf[5] = {0};
            
            if (parse_maps_line(line, &start, &end, perm_buf)) {

                // store information about the valid region
                starts[num_regions] = start;
                ends[num_regions] = end;
                strcpy(perms[num_regions], perm_buf);
                num_regions++;
            }
        }

        // retrieve next token
        line = strtok(NULL, "\n");
    }

    printf("Found %zu memory regions to include in image\n", num_regions);
    
    long page_size = sysconf(_SC_PAGESIZE);

    // calculate total virtual space size (page aligned)
    size_t VIRTUAL_SPACE_SIZE = 0;
    for (size_t i = 0; i < num_regions; i++) {
        size_t region_size = ends[i] - starts[i];
        VIRTUAL_SPACE_SIZE += region_size;
        printf("Region %zu: %lx-%lx (%s) Size: %zu bytes\n",
               i, starts[i], ends[i], perms[i], region_size);
        VIRTUAL_SPACE_SIZE = (VIRTUAL_SPACE_SIZE + page_size - 1) & ~(page_size - 1);
    }

    printf("Total size of memory regions: %zu bytes\n", VIRTUAL_SPACE_SIZE);

    // create output file
    int w_fd = open(output_filename, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (w_fd == -1) {
        perror("Error opening output file");
        return EXIT_FAILURE;
    }

    // calculate header size and align it to page boundary
    size_t header_size = sizeof(Header) + num_regions * sizeof(Entry);
    size_t aligned_header_size = (header_size + page_size - 1) & ~(page_size - 1);

    // compute total file size with per-region alignment
    size_t total_file_size = aligned_header_size;
    for (size_t i = 0; i < num_regions; i++) {
        size_t region_size = ends[i] - starts[i];
        total_file_size += region_size;
        total_file_size = (total_file_size + page_size - 1) & ~(page_size - 1);
    }

    // set the file size
    if (ftruncate(w_fd, total_file_size) == -1) {
        perror("Error truncating file");
        close(w_fd);
        return EXIT_FAILURE;
    }

    // map the file into memory
    void *map = mmap(NULL, total_file_size, PROT_READ | PROT_WRITE, MAP_SHARED, w_fd, 0);
    if (map == MAP_FAILED) {
        perror("Error mapping file");
        close(w_fd);
        return EXIT_FAILURE;
    }

    // fill in the header
    Header *header = (Header *)map;
    header->numEntries = num_regions;
    
    // store function pointers in header
    size_t funcs_to_store = (num_funcs > MAX_FUNC_PTRS) ? MAX_FUNC_PTRS : num_funcs;
    printf("Storing %zu function pointers in image header\n", funcs_to_store);
    
    // zero out the function pointers first
    for (int i = 0; i < MAX_FUNC_PTRS; i++) {
        header->func_ptr[i] = NULL;
    }
    
    // copy the provided function pointers
    for (size_t i = 0; i < funcs_to_store; i++) {
        header->func_ptr[i] = func_list[i];
        printf("Stored function pointer %zu at address %p\n", i, (void*)func_list[i]);
    }
    
    // initialize the current offset for data after the header
    size_t current_offset = aligned_header_size;

    // fill in entries and copy memory regions
    for (size_t i = 0; i < num_regions; i++) {

        // fill in metadata for this region
        header->entries[i].start = starts[i];
        header->entries[i].end = ends[i];
        header->entries[i].offsetIntoFile = current_offset;
        strcpy(header->entries[i].perms, perms[i]);

        size_t region_size = ends[i] - starts[i];

        // copy memory region if it has read permission
        if (perms[i][0] == 'r') {
            void *src_addr = (void *)starts[i];
            void *dest_addr = (void *)((char *)map + current_offset);

            memcpy(dest_addr, src_addr, region_size);
        }

        // update offset for next region
        current_offset += region_size;
        current_offset = (current_offset + page_size - 1) & ~(page_size - 1);
    }

    // final file size after all regions
    total_file_size = current_offset;

    // unmap/close the file
    if (munmap(map, total_file_size) == -1) {
        perror("Error unmapping file");
    }
    
    close(w_fd);
    
    printf("Memory snapshot created successfully in %s\n", output_filename);
    return EXIT_SUCCESS;
}

// determine if a memory region should be excluded
int should_exclude_region(const char *line) {
    return (strstr(line, "[vvar]")        != NULL ||
            strstr(line, "[vdso]")        != NULL ||
            strstr(line, "[vvar_vclock]") != NULL ||
            strstr(line, "[stack]")       != NULL ||
            strstr(line, "[vsyscall]")    != NULL   );
}

// parse a line from /proc/self/maps
int parse_maps_line(const char *line, ulong *start, ulong *end, char *perms) {
    // extract address range and permissions
    if (sscanf(line, "%lx-%lx %4s", start, end, perms) != 3) {
        return 0;  // failed to parse
    }
    return 1;
}
