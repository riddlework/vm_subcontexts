#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <sys/mman.h>
#include <stdint.h>

#define BUFSZ 16000
#define MAX_ENTRIES 1000  // max num of mem regions we can track

typedef struct entry {
    ulong start, end;
    ulong offsetIntoFile;
    char perms[5];  // store perms
} Entry;

typedef struct header {
    ulong numEntries;
    Entry entries[];  // flexible array size
} Header;

// exclude region based on name
int should_exclude_region(const char *line) {
    return (strstr(line, "[vvar]") != NULL ||
            strstr(line, "[vdso]") != NULL ||
            strstr(line, "[vvar_vclock]") != NULL ||
            strstr(line, "[stack]") != NULL ||
            strstr(line, "[vsyscall]") != NULL);
}

// parse a line from /proc/self/maps
int parse_maps_line(const char *line, ulong *start, ulong *end, char *perms) {
    // extract address range and permissions
    if (sscanf(line, "%lx-%lx %4s", start, end, perms) != 3) {
        return 0;  // Failed to parse
    } return 1;
}

// mmap the file into the new program using the data from the file
void dumpMemoryToFile() {
    // open /proc/self/maps
    int maps_fd = open("/proc/self/maps", O_RDONLY);

    // error checking
    if (maps_fd == -1) {
        perror("ERROR OPENING FILE");
        exit(EXIT_FAILURE);
    }

    char buf[BUFSZ];
    ssize_t bytes_read;

    // read the contents of the file to a buffer
    bytes_read = read(maps_fd, buf, BUFSZ - 1);
    
    if (bytes_read == -1) {
        perror("ERROR READING FILE");
        close(maps_fd);
        exit(EXIT_FAILURE);
    }
    
    if (bytes_read >= BUFSZ - 1) {
        fprintf(stderr, "Buffer too small for maps file\n");
        close(maps_fd);
        exit(EXIT_FAILURE);
    }
    
    buf[bytes_read] = '\0';
    close(maps_fd);

    // arrays for memory addrs, perms
    ulong starts[MAX_ENTRIES];
    ulong ends[MAX_ENTRIES];
    char perms[MAX_ENTRIES][5];
    size_t num_regions = 0;

    // parse the buffer
    char *line = strtok(buf, "\n");
    while (line && num_regions < MAX_ENTRIES) {
        // skip excluded regions
        if (!should_exclude_region(line)) {
            ulong start, end;
            char perm_buf[5] = {0};
            
            if (parse_maps_line(line, &start, &end, perm_buf)) {
                // store the valid region in the arrays
                starts[num_regions] = start;
                ends[num_regions] = end;
                strcpy(perms[num_regions], perm_buf);
                num_regions++;
            }
        } line = strtok(NULL, "\n");
    }

    // for debugging
    printf("Found %zu memory regions (excluding special regions)\n", num_regions);
    
    // calculate the size of the total virtual space taken up by mappings
    size_t VIRTUAL_SPACE_SIZE = 0;
    for (size_t i = 0; i < num_regions; i++) {
        size_t region_size = ends[i] - starts[i];
        VIRTUAL_SPACE_SIZE += region_size;
        printf("Region %zu: %lx-%lx (%s) Size: %zu bytes\n", 
               i, starts[i], ends[i], perms[i], region_size);
    }

    // more debugging printing
    printf("Total size of memory regions: %zu bytes\n", VIRTUAL_SPACE_SIZE);

    // create a file for metadata + mem dump
    int w_fd = open("./mem_dump", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (w_fd == -1) {
        perror("ERROR OPENING OUTPUT FILE");
        exit(EXIT_FAILURE);
    }

    // calculate the size of the header that stores metadata about mappings
    size_t header_size = sizeof(Header) + num_regions * sizeof(Entry);
    
    // align the header size on a page boundary
    size_t aligned_header_size = (header_size + 4095) & ~4095;

    // calculate the total size of the file
    size_t total_file_size = aligned_header_size + VIRTUAL_SPACE_SIZE;
    
    // truncate the file to the desired size
    if (ftruncate(w_fd, total_file_size) == -1) {
        perror("ERROR TRUNCATING FILE");
        close(w_fd);
        exit(EXIT_FAILURE);
    }

    // mmap the file
    void *map = mmap(NULL, total_file_size, PROT_READ | PROT_WRITE, MAP_SHARED, w_fd, 0);
    if (map == MAP_FAILED) {
        perror("ERROR MAPPING FILE");
        close(w_fd);
        exit(EXIT_FAILURE);
    }

    // fill in the header
    Header *header = (Header *) map;
    header->numEntries = num_regions;
    
    // initialize the current offset of the file after metadata writing
    ulong current_offset = aligned_header_size;
    
    // fill in entries and copy memory from appropriate regions
    for (size_t i = 0; i < num_regions; i++) {
        // fill in entry metadata
        header->entries[i].start = starts[i];
        header->entries[i].end   = ends[i];
        header->entries[i].offsetIntoFile = current_offset;
        
        // copy memory region if it has read permission
        if (perms[i][0] == 'r') {
            void *src_addr = (void *) starts[i];
            void *dest_addr = (void *) ((char *) map + current_offset);
            size_t region_size = ends[i] - starts[i];
            
            // copy the memory of the region
            if (memcpy(dest_addr, src_addr, region_size) != dest_addr) {
                fprintf(stderr, "Warning: Could not copy region %zu (%lx-%lx)\n", 
                        i, starts[i], ends[i]);
            }
        }
        
        // increment offset
        current_offset += (ends[i] - starts[i]);
    }

    // Sync changes to disk
    if (msync(map, total_file_size, MS_SYNC) == -1) {
        perror("ERROR SYNCING FILE");
    }

    // Unmap file 
    if (munmap(map, total_file_size) == -1) {
        perror("ERROR UNMAPPING FILE");
    }

    // Close file
    close(w_fd);
    
    printf("Memory dump completed successfully to ./mem\n");
}

int main() {
    dumpMemoryToFile();
    return 0;
}

// create a new process, map regions into new process
// should all be map shared
// use map fixed to fix the location
// if overlaps with process maps, throw a terminal error
// permissions that are important: private vs shared
// private: not shared with other processes
// shared: shared with other processes, won't be able to replicated in remapping process (report these as an error)

// after this, dummy function in original map process
// record the memory address of this function and then try to call the function from the new process


// the purpose of this first tool is to create a subcontext
// the second tool creates a library that becomes the client of the subcontext and opens the subcontext
// part of starting the process is getting a list of function pointers provided by the first tool that you should be able to call
