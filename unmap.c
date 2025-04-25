#include "vm_sbc.h"
#include <stdio.h>

extern void function1(void);
extern void function2(void);

// check if a region overlaps with any of the process's existing mmaps
int check_for_overlap(unsigned long start, unsigned long end) {
    // use fopen instead of open here so that we can use fgets
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

// convert permission string to mmap prot flags
int perms_to_prot(const char *perm) {
    int prot = 0;
    if (perm[0] == 'r') prot |= PROT_READ;
    if (perm[1] == 'w') prot |= PROT_WRITE;
    if (perm[2] == 'x') prot |= PROT_EXEC;
    return prot;
}

int main() {
    const char *filename = "./mem_dump";
    
    // open memory dump file
    int fd = open(filename, O_RDWR);
    if (fd == -1) {
        perror("Error opening memory dump file");
        return EXIT_FAILURE;
    }
    
    // get file size
    // TODO: use stat or only map the first page for metadata
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
    /*printf("metadata_map: %p\n", metadata_map);*/
    if (metadata_map == MAP_FAILED) {
        perror("Error mapping file");
        close(fd);
        return EXIT_FAILURE;
    }
    
    // read the file metadata
    Header *header = (Header *) metadata_map;
    unsigned long num_entries = header->numEntries;
    
    printf("Memory dump contains %lu memory regions\n", num_entries);
    
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

        // copy over permissions
        char perms[5] = {0};
        strncpy(perms, entry->perms, 4);

        // add to exec_regions if executable
        if (perms[2] == 'x') exec_regions[num_exec_regions++] = i;
    }
    
    
    printf("No overlapping regions detected. Proceeding with mapping...\n");
    
    // Now map each region into memory
    for (int i = 0; i < num_entries; i++) {
        Entry *entry = &header->entries[i];

        // copy over permissions
        char perms[5] = {0};
        strncpy(perms, entry->perms, 4);
        
        size_t region_size = entry->end - entry->start;
        
        // convert permissions string to mmap prot flags
        int prot = perms_to_prot(perms);
        
        // calculate file offset for this region
        off_t file_offset = entry->offsetIntoFile;
        
        // debugging: what region are we mapping?
        printf("Mapping region %d: %016lx-%016lx; Size: %zu bytes; Perms: %s; Offset: %lu\n", 
               i,
               entry->start,
               entry->end,
               region_size,
               perms,
               file_offset
               );
        
        // map the region into memory -- map fixed gives a hint about where to put it
        void *region_map = mmap((void*) entry->start, region_size, prot, 
                               MAP_SHARED | MAP_FIXED, fd, file_offset);
        
        if (region_map == MAP_FAILED) {
            perror("Error mapping memory region");
            fprintf(stderr, "Failed to map region %d at address %016lx\n", i, entry->start);
            munmap(metadata_map, file_size);
            close(fd);
            return EXIT_FAILURE;
        }

        // debugging: check if the region was mapped in the correct location
        if (region_map != (void*) entry->start) {
            fprintf(stderr, "Warning: Region %d mapped at %p instead of requested %016lx\n", 
                    i, region_map, entry->start);
        } else {
            printf("Successfully mapped region %d at address %016lx\n", i, entry->start);
        }
    }

    // trying to find a function to call in the executable regions
    if (num_exec_regions > 0) {
        printf("Found %zu executable regions. Attempting to call a function...\n", num_exec_regions);
        
        // TODO: how do we actually know where the function is?
        // let's try calling a function from the first executable region
        // do we need to know the exact address of the function?
        
        // re-mmap and unpack the header for this region again
        void *header_map = mmap(NULL, sizeof(Header) + num_entries * sizeof(Entry), 
                                PROT_READ, MAP_PRIVATE, fd, 0);

        // error checking
        if (header_map == MAP_FAILED) {
            perror("Error remapping header");
            close(fd);
            return EXIT_FAILURE;
        }
        
        // cast to the appropriate pointer
        /*Header *h = (Header *) header_map;*/
        /*unsigned long exec_index = exec_regions[0];*/
        /*unsigned long func_addr = h->entries[exec_index].start;*/
        
        printf("Attempting to call function at address: %016lx\n", header->func_ptr[0]);
        printf("Attempting to call function at address: %016lx\n", header->func_ptr[1]);
        
        // try calling the function
        printf("Calling mapped function...\n");
        header->func_ptr[0](0);
        /*sleep(5);*/
        header->func_ptr[1](0);
        printf("Function call returned successfully\n");
        
        // unmap the header again now that we're done with it 
        munmap(header_map, sizeof(Header) + num_entries * sizeof(Entry));
    } else {
        printf("No executable regions found. Cannot call any functions.\n");
    }
    
    // unmap metadata
    munmap(metadata_map, file_size);
    
    // debugging: if we've gotten here, all memory regions were mapped
    printf("Successfully mapped all memory regions from %s\n", filename);

    char dummy_buf[256];
    sprintf(dummy_buf, "ls -al mem_dump; cat /proc/%d/maps", getpid());
    system(dummy_buf);
    
    close(fd);
    
    return EXIT_SUCCESS;
}

// make the filename for memory dump a parameter
