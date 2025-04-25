#include "vm_sbc.h"

/**
 * maps a memory image file into the current process's address space.
 * this effectively creates a "subcontext" within the current process.
 *
 * @param image_file The path to the image file to map
 * @return 0 on success, non-zero on failure
 */
int map_subcontext(const char *image_file) {
    printf("Mapping subcontext from file: %s\n", image_file);
    
    // open the image file
    int fd = open(image_file, O_RDWR);
    if (fd == -1) {
        perror("Error opening image file");
        return EXIT_FAILURE;
    }
    
    // get file size
    off_t file_size = lseek(fd, 0, SEEK_END);
    if (file_size == -1) {
        perror("Error determining file size");
        close(fd);
        return EXIT_FAILURE;
    }
    
    // reset file position to beginning
    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("Error resetting file position");
        close(fd);
        return EXIT_FAILURE;
    }
    
    // map the file into memory to read metadata
    void *metadata_map = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (metadata_map == MAP_FAILED) {
        perror("Error mapping file for metadata");
        close(fd);
        return EXIT_FAILURE;
    }
    
    // read the file metadata
    Header *header = (Header *)metadata_map;
    unsigned long num_entries = header->numEntries;
    
    printf("Image contains %lu memory regions\n", num_entries);
    
    // keep track of executable regions
    int exec_regions[MAX_ENTRIES];
    size_t num_exec_regions = 0;

    // check all regions for overlaps with current process
    for (int i = 0; i < num_entries; i++) {
        Entry *entry = &header->entries[i];
        
        if (check_for_overlap(entry->start, entry->end)) {
            fprintf(stderr, "Fatal error: Region %d (%016lx-%016lx) overlaps with existing process memory.\n",
                    i, entry->start, entry->end);
            munmap(metadata_map, file_size);
            close(fd);
            return EXIT_FAILURE;
        }

        // track executable regions
        char perms[5] = {0};
        strncpy(perms, entry->perms, 4);
        if (perms[2] == 'x') {
            exec_regions[num_exec_regions++] = i;
        }
    }
    
    printf("No overlapping regions detected. Proceeding with mapping...\n");
    
    // map each region into memory
    for (int i = 0; i < num_entries; i++) {
        Entry *entry = &header->entries[i];

        // retrieve permissions
        char perms[5] = {0};
        strncpy(perms, entry->perms, 4);
        
        size_t region_size = entry->end - entry->start;
        
        // convert permissions string to mmap prot flags
        int prot = perms_to_prot(perms);
        
        // calculate file offset for this region
        off_t file_offset = entry->offsetIntoFile;
        
        printf("Mapping region %d: %016lx-%016lx; Size: %zu bytes; Perms: %s; Offset: %lu\n", 
               i,
               entry->start,
               entry->end,
               region_size,
               perms,
               file_offset);
        
        // map the region into memory at the specific address
        void *region_map = mmap((void*)entry->start, region_size, prot, 
                               MAP_SHARED | MAP_FIXED, fd, file_offset);
        
        if (region_map == MAP_FAILED) {
            perror("Error mapping memory region");
            fprintf(stderr, "Failed to map region %d at address %016lx\n", i, entry->start);
            munmap(metadata_map, file_size);
            close(fd);
            return EXIT_FAILURE;
        }

        // verify mapping address
        if (region_map != (void*)entry->start) {
            fprintf(stderr, "Warning: Region %d mapped at %p instead of requested %016lx\n", 
                    i, region_map, entry->start);
        } else {
            printf("Successfully mapped region %d at address %016lx\n", i, entry->start);
        }
    }

    // store function poitners from subcontext image here
    void (*func_ptr[MAX_FUNC_PTRS])(int);
    for (int i = 0; i < MAX_FUNC_PTRS; i++) {
        func_ptr[i] = header->func_ptr[i];
    }
    
    // unmap metadata mapping
    munmap(metadata_map, file_size);
    
    // DEBUGGING: report on executable regions and function pointers??
    if (num_exec_regions > 0) {
        printf("Found %zu executable regions.\n", num_exec_regions);

        printf("Available functions from subcontext:\n");
        for (int i = 0; i < MAX_FUNC_PTRS; i++) {
            if (func_ptr[i] != NULL) {
                printf("Function %d at address: %p\n", i, func_ptr[i]);
            }
        }
    } else {
        printf("No executable regions found. Cannot call any functions.\n");
    }
    
    printf("Successfully mapped all memory regions from %s\n", image_file);
    
    // Keep the file descriptor open as long as the mappings are needed
    // The caller should eventually close this FD when done with the subcontext
    return fd;
}

/**
 * Check if a region overlaps with any of the process's existing mmaps
 */
int check_for_overlap(unsigned long start, unsigned long end) {
    // open the current process's memory map
    FILE *maps_file = fopen("/proc/self/maps", "r");
    if (!maps_file) {
        perror("Error opening /proc/self/maps");
        return -1;
    }
    
    char line[256];
    int has_overlap = 0;
    
    while (fgets(line, sizeof(line), maps_file) != NULL) {
        unsigned long map_start, map_end;
        if (sscanf(line, "%lx-%lx", &map_start, &map_end) == 2) {
            // Check for overlap
            if ((start <= map_end) && (end >= map_start)) {
                has_overlap = 1;
                printf("Overlap detected: %016lx-%016lx overlaps with %016lx-%016lx\n", 
                       start, end, map_start, map_end);
                break;
            }
        }
    }
    
    fclose(maps_file);
    return has_overlap;
}

/**
 * Convert permission string to mmap prot flags
 */
int perms_to_prot(const char *perm) {
    int prot = 0;
    if (perm[0] == 'r') prot |= PROT_READ;
    if (perm[1] == 'w') prot |= PROT_WRITE;
    if (perm[2] == 'x') prot |= PROT_EXEC;
    return prot;
}

/**
 * Call a function from the mapped subcontext
 */
int call_subcontext_function(int func_index, int fd) {
    // map just the header to get the function pointer
    Header *header;
    void *header_map = mmap(NULL, sizeof(Header), PROT_READ, MAP_PRIVATE, fd, 0);
    
    if (header_map == MAP_FAILED) {
        perror("Error mapping header for function call");
        return EXIT_FAILURE;
    }
    
    header = (Header *)header_map;
    
    // ERROR CHECKING TODO: CHECK FUNCTION INDEX??
    // check if the function index is valid
    /*if (func_index < 0 || func_index >= MAX_FUNCTION_PTRS || header->func_ptr[func_index] == NULL) {*/
    /*    fprintf(stderr, "Invalid function index or NULL function pointer\n");*/
    /*    munmap(header_map, sizeof(Header));*/
    /*    return EXIT_FAILURE;*/
    /*}*/
    
    // Get the function pointer
    void (*func)(int) = header->func_ptr[func_index];
    
    printf("Calling function at address: %p\n", func);
    
    // Call the function
    func(0);
    
    // Clean up
    munmap(header_map, sizeof(Header));
    
    return EXIT_SUCCESS;
}

// include a main function here for testing???
#ifdef SBC_CLIENT_MAIN
int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <image_file> [function_index]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    int fd = map_subcontext(argv[1]);
    if (fd < 0) {
        return EXIT_FAILURE;
    }
    
    // If function index is provided, call that function
    if (argc >= 3) {
        int func_index = atoi(argv[2]);
        call_subcontext_function(func_index, fd);
    }
    
    // Keep mappings active until user presses a key
    printf("Subcontext mapped. Press Enter to continue...\n");
    getchar();
    
    close(fd);
    return EXIT_SUCCESS;
}
#endif
