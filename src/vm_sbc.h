#ifndef _VM_SBC_H
#define _VM_SBC_H

#include <sys/mman.h>
#include <sys/types.h>
#include <signal.h>

// buffer size for reading /proc/self/maps
#define BUFSZ 16000

// for reading smaller things
#define SMLBUFSZ 256

// max number of memory regions to map
#define MAX_ENTRIES 1024

// max num of function pointers to store
#define MAX_FUNC_PTRS 16

// max num of image files that a client process can map
#define MAX_IMG_FILES 32

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

// data structure to track mapped subcontexts
typedef struct mapped_subcontext {
    char    img_file[256];
    int     fd;
    void   *base_addr;
    size_t  total_size;
    Entry  *entries;
    size_t  num_entries;
    Header *header;
    int     is_active;  // a flag indicating whether this subcontext is currently executable
} MappedSubcontext;

// client process memory regions
// use for re-enabling client permissions
typedef struct client_region {
    void *start;
    void *end;
    int original_prot;
} ClientRegion;

/* global state maintained in sbc_mm.c */
extern MappedSubcontext mapped_subcontexts[MAX_IMG_FILES];
extern size_t          num_mapped_subcontexts;
extern ClientRegion    client_regions[MAX_ENTRIES];
extern size_t          num_client_regions;

// prototypes

/* for server processes */
int create_image_file(const char *filename, void (**func_list)(int), size_t num_funcs);

/* for client processes */
int map_subcontext(const char *filename); // client
int call_subcontext_function(int func_idx, int fd);
int unmap_subcontext(int fd);
int setup_segv_handler(void);
int disable_client_execute_permissions(void);
int enable_client_execute_permissions(void);
int enable_subcontext_execute_permissions(void *fault_addr);
int disable_all_subcontext_execute_permissions(void);
MappedSubcontext* find_subcontext_by_addr(void *addr);
int record_client_memory_regions(void);
int is_library_address(void *addr);
void sbc_client_init(void);

/* for the match maker */
void init();
int request_map(const char *img_fname);
void finalize();
void mm_handle_segv(void *fault_addr);

/* for use by server/client libraries */
int check_for_overlap(unsigned long start, unsigned long end);
int perms_to_prot(const char *perm);
int should_exclude_region(const char *line);
int parse_maps_line(const char *line, ulong *start, ulong *end, char *perms);

#endif
