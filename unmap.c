#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>

// Ensure these structures match those in maps.c
typedef struct entry {
    unsigned long start, end;
    unsigned long offsetIntoFile;
    char perms[5];  // We'll use default permissions if not available in the file
} Entry;

typedef struct header {
    unsigned long numEntries;
    Entry entries[];  // Flexible array member
} Header;

// Helper function to convert permissions bits to string representation
void permissions_to_string(char *perm_str) {
    // If we don't have stored permissions, use a default of "rw-p"
    if (perm_str[0] == '\0') {
        strcpy(perm_str, "rw-p");
    }
}

int main(int argc, char *argv[]) {
    const char *filename = "./mem";
    const char *output_filename = "./reconstructed_maps";
    
    // Override defaults if provided
    if (argc > 1) {
        filename = argv[1];
    }
    if (argc > 2) {
        output_filename = argv[2];
    }
    
    // Open the memory dump file
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        perror("Error opening memory dump file");
        return EXIT_FAILURE;
    }
    
    // Get file size
    off_t file_size = lseek(fd, 0, SEEK_END);
    if (file_size == -1) {
        perror("Error determining file size");
        close(fd);
        return EXIT_FAILURE;
    }
    
    // Reset position to beginning of file
    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("Error resetting file position");
        close(fd);
        return EXIT_FAILURE;
    }
    
    // Map the file into memory
    void *map = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (map == MAP_FAILED) {
        perror("Error mapping file");
        close(fd);
        return EXIT_FAILURE;
    }
    
    // Read the header
    Header *header = (Header *)map;
    unsigned long num_entries = header->numEntries;
    
    printf("Memory dump contains %lu memory regions\n", num_entries);
    
    // Open output file
    FILE *outfile = fopen(output_filename, "w");
    if (!outfile) {
        perror("Error opening output file");
        munmap(map, file_size);
        close(fd);
        return EXIT_FAILURE;
    }
    
    // Process each entry and write to output file
    for (unsigned long i = 0; i < num_entries; i++) {
        Entry *entry = &header->entries[i];
        
        char perms[5] = {0};
        // Check if we have stored permissions
        if (i < num_entries && entry->perms[0] != '\0') {
            strncpy(perms, entry->perms, 4);
        } else {
            // Assume read-write permissions if not stored
            strcpy(perms, "rw-p");
        }
        
        // Format similar to /proc/self/maps
        // address range, permissions, offset, dev, inode, pathname
        fprintf(outfile, "%016lx-%016lx %s %08lx 00:00 0 ", 
                entry->start, entry->end, perms, 0UL);
        
        // Add a placeholder name
        fprintf(outfile, "      [region_%lu]\n", i);
        
        printf("Region %lu: %016lx-%016lx Size: %lu bytes\n", 
               i, entry->start, entry->end, entry->end - entry->start);
    }
    
    // Close everything
    fclose(outfile);
    munmap(map, file_size);
    close(fd);
    
    printf("Successfully reconstructed memory map to %s\n", output_filename);
    
    return EXIT_SUCCESS;
}

