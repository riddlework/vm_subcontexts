#ifndef _VM_SBC_H
#define _VM_SBC_H

#include <sys/mman.h>
#include <sys/types.h>

// buffer size for reading /proc/self/maps
#define BUFSZ 16000

// for reading smaller things
#define SMLBUFSZ 256

// max number of memory regions to map
#define MAX_ENTRIES 1024

// max num of function pointers to store
#define MAX_FUNC_PTRS 16

typedef struct entry {
    ulong start, end;
    ulong offsetIntoFile;
    char perms[5];  // store perms
} Entry;

// TODO: make this more flexible?
typedef struct header {
    void (*func_ptr[MAX_FUNC_PTRS])(int);
    ulong numEntries;
    Entry entries[MAX_ENTRIES];
} Header;

// prototypes
int create_image_file(const char *filename, void (**func_list)(int), size_t num_funcs);
int map_subcontext(const char *filename);
int call_subcontext_function(int func_index, int fd);

int check_for_overlap(unsigned long start, unsigned long end);
int perms_to_prot(const char *perm);
int should_exclude_region(const char *line);
int parse_maps_line(const char *line, ulong *start, ulong *end, char *perms);

#endif
