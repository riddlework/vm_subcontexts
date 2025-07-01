#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>
#include <sys/mman.h>
#include <ucontext.h>
#include "vm_sbc.h"

static MappedSubcontext mapped_subcontexts[MAX_IMG_FILES];
static size_t num_mapped_subcontexts = 0;
static int segv_handler_installed = 0;
static ClientRegion client_regions[MAX_ENTRIES];
static size_t num_client_regions = 0;

// Initialize the client - set up signal handler
void init() {
    printf("Initializing SBC client...\n");
    
    // Initialize global structures
    memset(mapped_subcontexts, 0, sizeof(mapped_subcontexts));
    memset(client_regions, 0, sizeof(client_regions));
    num_mapped_subcontexts = 0;
    num_client_regions = 0;
    
    // Record current client memory regions for later permission management
    if (record_client_memory_regions() != 0) {
        fprintf(stderr, "Warning: Failed to record client memory regions\n");
    }
    
    // Set up the segmentation fault handler
    if (setup_segv_handler() != 0) {
        fprintf(stderr, "Error: Failed to set up segmentation fault handler\n");
        exit(EXIT_FAILURE);
    }
    
    printf("SBC client initialized successfully\n");
}

/**
 * maps a memory image file into the current process's address space with NO permissions.
 * this effectively creates a "subcontext" within the current process.
 *
 * @param img_file The path to the image file to map
 * @return 0 on success, non-zero on failure
 */
int map_subcontext(const char *img_file) {
    printf("Mapping subcontext from file: %s\n", img_file);
    
    if (num_mapped_subcontexts >= MAX_IMG_FILES) {
        fprintf(stderr, "Error: Maximum number of subcontexts already mapped\n");
        return EXIT_FAILURE;
    }
    
    // open the image file
    int fd = open(img_file, O_RDWR);
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
    
    // Read the file metadata
    Header *header = (Header *)metadata_map;
    unsigned long num_entries = header->numEntries;
    
    printf("Image contains %lu memory regions\n", num_entries);
    
    // Check all regions for overlaps with current process
    for (int i = 0; i < num_entries; i++) {
        Entry *entry = &header->entries[i];
        
        if (check_for_overlap(entry->start, entry->end)) {
            fprintf(stderr, "Fatal error: Region %d (%016lx-%016lx) overlaps with existing process memory.\n",
                    i, entry->start, entry->end);
            munmap(metadata_map, file_size);
            close(fd);
            return EXIT_FAILURE;
        }
    }
    
    printf("No overlapping regions detected. Proceeding with mapping...\n");
    
    // Initialize the subcontext structure
    MappedSubcontext *subctx = &mapped_subcontexts[num_mapped_subcontexts];
    strncpy(subctx->img_file, img_file, sizeof(subctx->img_file) - 1);
    subctx->img_file[sizeof(subctx->img_file) - 1] = '\0';
    subctx->fd = fd;
    subctx->num_entries = num_entries;
    subctx->is_active = 0;
    
    // Allocate memory for entries
    subctx->entries = malloc(num_entries * sizeof(Entry));
    if (!subctx->entries) {
        perror("Error allocating memory for entries");
        munmap(metadata_map, file_size);
        close(fd);
        return EXIT_FAILURE;
    }
    
    // Copy entries
    memcpy(subctx->entries, header->entries, num_entries * sizeof(Entry));
    
    // Store a copy of the header for function pointers
    subctx->header = malloc(sizeof(Header));
    if (!subctx->header) {
        perror("Error allocating memory for header");
        free(subctx->entries);
        munmap(metadata_map, file_size);
        close(fd);
        return EXIT_FAILURE;
    }
    memcpy(subctx->header, header, sizeof(Header));
    
    // Map each region into memory with NO permissions initially
    for (int i = 0; i < num_entries; i++) {
        Entry *entry = &subctx->entries[i];
        
        size_t region_size = entry->end - entry->start;
        off_t file_offset = entry->offsetIntoFile;
        
        printf("Mapping region %d: %016lx-%016lx; Size: %zu bytes; Offset: %lu (NO PERMISSIONS)\n", 
               i, entry->start, entry->end, region_size, file_offset);
        
        void *region_map = mmap((void*)entry->start, region_size, PROT_READ | PROT_WRITE, 
                               MAP_SHARED | MAP_FIXED, fd, file_offset);

        // disable permissions
        mprotect((void *)entry->start, region_size, PROT_NONE);
        
        if (region_map == MAP_FAILED) {
            perror("Error mapping memory region");
            fprintf(stderr, "Failed to map region %d at address %016lx\n", i, entry->start);
            // clean previously mapped regions
            for (int j = 0; j < i; j++) {
                Entry *prev_entry = &subctx->entries[j];
                munmap((void*)prev_entry->start, prev_entry->end - prev_entry->start);
            }
            free(subctx->entries);
            free(subctx->header);
            munmap(metadata_map, file_size);
            close(fd);
            return EXIT_FAILURE;
        }
        
        if (region_map != (void*)entry->start) {
            fprintf(stderr, "Warning: Region %d mapped at %p instead of requested %016lx\n", 
                    i, region_map, entry->start);
        } else {
            printf("Successfully mapped region %d at address %016lx (no permissions)\n", i, entry->start);
        }
    }
    
    // record the base address and total size
    if (num_entries > 0) {
        subctx->base_addr = (void*)subctx->entries[0].start;
        subctx->total_size = subctx->entries[num_entries - 1].end - subctx->entries[0].start;
    }
    
    // unmap metadata mapping
    munmap(metadata_map, file_size);
    
    // add to global list
    num_mapped_subcontexts++;
    
    printf("Successfully mapped subcontext from %s (index %zu)\n", img_file, num_mapped_subcontexts - 1);
    
    return 0;
}

/**
 * set up seg fault handler -- use sigaction
 */
static int setup_segv_handler(void) {
    if (segv_handler_installed) {
        return 0; // Already installed
    }
    
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    
    sa.sa_sigaction = segv_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("Error installing SIGSEGV handler");
        return -1;
    }
    
    segv_handler_installed = 1;
    printf("SIGSEGV handler installed successfully\n");
    return 0;
}

/**
 * Segmentation fault handler - manages permission switching between client and subcontexts
 */
static void segv_handler(int sig, siginfo_t *info, void *context) {
    // TODO: check for type of access -- should only intercept execute
    void *fault_addr = info->si_addr;
    
    printf("SEGV handler triggered at address: %p\n", fault_addr);
    
    // check if the fault address is in a library, as we should not disable library permissions
    if (is_library_address(fault_addr)) {
        printf("Fault in library code - allowing normal execution\n");
        return;
    }
    
    // Find which subcontext contains this address
    MappedSubcontext *target_subctx = find_subcontext_by_addr(fault_addr);
    
    if (target_subctx) {
        printf("Fault in subcontext %s - switching permissions\n", target_subctx->img_file);
        
        // Disable execute permissions on client (except libraries)
        if (disable_client_execute_permissions() != 0) {
            fprintf(stderr, "Error: Failed to disable client execute permissions\n");
            exit(EXIT_FAILURE);
        }
        
        // Disable execute permissions on all other subcontexts
        if (disable_all_subcontext_execute_permissions() != 0) {
            fprintf(stderr, "Error: Failed to disable other subcontext permissions\n");
            exit(EXIT_FAILURE);
        }
        
        // Enable execute permissions on the target subcontext
        if (enable_subcontext_execute_permissions(fault_addr) != 0) {
            fprintf(stderr, "Error: Failed to enable target subcontext permissions\n");
            exit(EXIT_FAILURE);
        }
        
        printf("Permission switch completed - target subcontext now executable\n");
    } else {
        // Fault not in any subcontext - might be returning to client
        printf("Fault not in subcontext - switching back to client\n");
        
        // Disable all subcontext execute permissions
        if (disable_all_subcontext_execute_permissions() != 0) {
            fprintf(stderr, "Error: Failed to disable subcontext permissions\n");
        }
        
        // Re-enable client execute permissions
        if (enable_client_execute_permissions() != 0) {
            fprintf(stderr, "Error: Failed to re-enable client execute permissions\n");
        }
        
        printf("Switched back to client execution\n");
    }
}

/**
 * Record current client memory regions for permission management
 */
static int record_client_memory_regions(void) {
    FILE *maps_file = fopen("/proc/self/maps", "r");
    if (!maps_file) {
        perror("Error opening /proc/self/maps");
        return -1;
    }
    
    char line[256];
    num_client_regions = 0;
    
    while (fgets(line, sizeof(line), maps_file) != NULL && num_client_regions < MAX_ENTRIES) {
        unsigned long start, end;
        char perms[5];
        
        if (sscanf(line, "%lx-%lx %4s", &start, &end, perms) == 3) {
            // Only record executable regions
            if (perms[2] == 'x') {
                client_regions[num_client_regions].start = (void*)start;
                client_regions[num_client_regions].end = (void*)end;
                client_regions[num_client_regions].original_prot = perms_to_prot(perms);
                num_client_regions++;
            }
        }
    }
    
    fclose(maps_file);
    printf("Recorded %zu client executable regions\n", num_client_regions);
    return 0;
}

/**
 * Disable execute permissions on client regions (except libraries)
 */
static int disable_client_execute_permissions(void) {
    for (size_t i = 0; i < num_client_regions; i++) {
        ClientRegion *region = &client_regions[i];
        
        // Skip library regions
        if (is_library_address(region->start)) {
            continue;
        }
        
        size_t size = (char*)region->end - (char*)region->start;
        int new_prot = region->original_prot & ~PROT_EXEC;
        
        if (mprotect(region->start, size, new_prot) == -1) {
            perror("Error disabling client execute permissions");
            return -1;
        }
    }
    
    return 0;
}

/**
 * Re-enable execute permissions on client regions
 */
static int enable_client_execute_permissions(void) {
    for (size_t i = 0; i < num_client_regions; i++) {
        ClientRegion *region = &client_regions[i];
        size_t size = (char*)region->end - (char*)region->start;
        
        if (mprotect(region->start, size, region->original_prot) == -1) {
            perror("Error re-enabling client execute permissions");
            return -1;
        }
    }
    
    return 0;
}

/**
 * Enable execute permissions on the subcontext containing the fault address
 */
static int enable_subcontext_execute_permissions(void *fault_addr) {
    MappedSubcontext *subctx = find_subcontext_by_addr(fault_addr);
    if (!subctx) {
        return -1;
    }
    
    // Enable permissions for all regions in this subcontext
    for (size_t i = 0; i < subctx->num_entries; i++) {
        Entry *entry = &subctx->entries[i];
        size_t region_size = entry->end - entry->start;
        int prot = perms_to_prot(entry->perms);
        
        if (mprotect((void*)entry->start, region_size, prot) == -1) {
            perror("Error enabling subcontext permissions");
            return -1;
        }
    }
    
    subctx->is_active = 1;
    return 0;
}

/**
 * Disable execute permissions on all mapped subcontexts
 */
static int disable_all_subcontext_execute_permissions(void) {
    for (size_t i = 0; i < num_mapped_subcontexts; i++) {
        MappedSubcontext *subctx = &mapped_subcontexts[i];
        
        for (size_t j = 0; j < subctx->num_entries; j++) {
            Entry *entry = &subctx->entries[j];
            size_t region_size = entry->end - entry->start;
            
            if (mprotect((void*)entry->start, region_size, PROT_NONE) == -1) {
                perror("Error disabling subcontext permissions");
                return -1;
            }
        }
        
        subctx->is_active = 0;
    }
    
    return 0;
}

/**
 * find the subcontext containing the given address
 */
static MappedSubcontext* find_subcontext_by_addr(void *addr) {
    for (size_t i = 0; i < num_mapped_subcontexts; i++) {
        MappedSubcontext *subctx = &mapped_subcontexts[i];
        
        for (size_t j = 0; j < subctx->num_entries; j++) {
            Entry *entry = &subctx->entries[j];
            if ((unsigned long)addr >= entry->start && (unsigned long)addr < entry->end) {
                return subctx;
            }
        }
    }
    
    return NULL;
}

/**
 * check if a given address is in a library to avoid it
 */
static int is_library_address(void *addr) {
    FILE *maps_file = fopen("/proc/self/maps", "r");
    if (!maps_file) {
        return 0;
    }
    
    char line[256];
    int is_lib = 0;
    
    while (fgets(line, sizeof(line), maps_file) != NULL) {
        unsigned long start, end;
        if (sscanf(line, "%lx-%lx", &start, &end) == 2) {
            if ((unsigned long)addr >= start && (unsigned long)addr < end) {
                // check if line contains library indicators
                if (strstr(line, ".so") || strstr(line, "libc") || strstr(line, "ld-")) {
                    is_lib = 1;
                }
                break;
            }
        }
    }
    
    fclose(maps_file);
    return is_lib;
}

/**
 * Check if a region overlaps with any of the process's existing mmaps
 */
int check_for_overlap(unsigned long start, unsigned long end) {
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
