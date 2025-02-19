#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>
#include <sys/mman.h>
#include <stdint.h>

#define BUFSZ 16000

typedef struct entry {
    ulong start,end;
    ulong offsetIntoFile;
} Entry;

typedef struct header {
    ulong numEntries;
    Entry entries[4096/sizeof(Entry)];
} Header;

// TODO: use union for character buffer/meta data structure

// mmap the file into the new program using the data from the file
void dumpMemoryToFile() {
    // retrieve id of current process
    // only need the pid if im transferring control to another tool that needs to access the same process
    pid_t cur_pid = getpid();

    // open the file /proc/self/maps
    int maps_fd = open("/proc/self/maps", O_RDONLY);

    // error checking
    if (maps_fd == -1) {
        perror("ERROR OPENING FILE");
        exit(EXIT_FAILURE);
    }

    char buf[BUFSZ];
    ssize_t bytes_read;

    // write the contents of the file to a buffer
    while ((bytes_read = read(maps_fd, buf, BUFSZ)) > 0) {

        // check if buffer is full
        if (bytes_read > BUFSZ) {
            perror("TOO MUCH DATA\n");
            exit(EXIT_FAILURE);
        }

        buf[bytes_read] = '\0';
    }

    // parse the buffer
    // TODO: use one buffer
    size_t num_addrs = 0;
    unsigned long addrs[100];
    unsigned long *addr_ptr = addrs;
    char cpy[BUFSZ];
    char *ptr = buf;
    char *cpy_ptr = cpy;
    

    while ((*cpy_ptr++ = *ptr++)) {
        if (*ptr == '-') {

            num_addrs++;
            *cpy_ptr = '\0';
            *addr_ptr++ = strtoul(cpy, NULL, 16);
            cpy_ptr = cpy;
            ptr++;

        } else if (*ptr == ' ') {

            num_addrs++;
            *cpy_ptr = '\0';
            *addr_ptr++ = strtoul(cpy, NULL, 16);
            cpy_ptr = cpy;

           while ((*ptr++ != '\n')) {} 
        } 
    }

    /*for (int i = 0; i < num_addrs; i++) printf("%016lx\n", addrs[i]);*/
    /*printf("%016lx\n", addrs[num_addrs]);*/

    // error checking
    if (bytes_read == -1) {
        perror("ERROR READING FILE");
    }

    // close reading file
    close(maps_fd);

    // check that the number of addresses is a multiple of two
    if (num_addrs % 2 != 0) {
        fprintf(stderr, "ERROR: Number of memory ranges not a multiple of 2!");
        exit(EXIT_FAILURE);
    }

    printf("num_addrs[25], num_addrs[24]: %ld, %ld\n", addrs[25], addrs[24]);
    printf("num_addrs[25] - num_addrs[24] = %ld\n", addrs[25] - addrs[25]);

    // calculate size of total virtual space
    size_t VIRTUAL_SPACE_SIZE = 0;
    for (int i = 0; i < num_addrs; i += 2) {
        VIRTUAL_SPACE_SIZE += (size_t) addrs[i+1] - (size_t) addrs[i];
    }

    printf("SIZE OF VIRTUAL SPACE: %d\n", (int) VIRTUAL_SPACE_SIZE);

    // open write file
    int w_fd = open("./mem", O_RDWR | O_CREAT | O_TRUNC, 0644);

    // error checking
    if (w_fd == -1) {
        perror("ERROR OPENING FILE");
        exit(EXIT_FAILURE);
    }

    // truncate the file to the desired size
    if (ftruncate(w_fd, VIRTUAL_SPACE_SIZE) == -1) {
        perror("ftruncate");
        close(w_fd);
        exit(EXIT_FAILURE);
    }

    // map the file
    size_t TOTAL_SIZE = 4096 + VIRTUAL_SPACE_SIZE;
    char *map = mmap(NULL, TOTAL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, w_fd, 4096);

    // error check the mapping
    if (map == MAP_FAILED) {
        perror("mmap");
        close(w_fd);
        exit(EXIT_FAILURE);
    }

    // dump memory regions to file-mapped memory
    char *map_ptr = map;

    for (int i = 0; i < num_addrs; i += 2) {
        void *start = (void *) addrs[i];
        size_t dump_size = (size_t) addrs[i+1] - (size_t) addrs[i];

        if (i < 8)
            printf("%x %x\n", addrs[i], addrs[i+1]);

        if ((uintptr_t) map_ptr % sizeof(long) != 0) {
            fprintf(stderr, "ERROR: Memory is not properly aligned!\n");
            close(w_fd);
            exit(1);
        }

        if (dump_size > 0) {
            // dump memory
            memcpy(map_ptr, start, dump_size);
            map_ptr += dump_size;
        } else {
            // invalid range
            fprintf(stderr, "ERROR: Invalid range -- Skipping these addresses");
        }
    }

    // sync changes to file
    if (msync(map, TOTAL_SIZE, MS_SYNC) == -1) perror("msync");

    // unmap memory
    if (munmap(map, TOTAL_SIZE) == -1) perror("munmap");

    // close write file
    close(w_fd);
}

int main() {
    dumpMemoryToFile();
    return 0;
}

// questions:
// how likely is it that a process uses memory map 0x0000000000000000?
// so if I encounter this address, can I assume it is an empty slot?
// I'm not using cur_pid, just accessing proc/self/maps -- is this a problem?
// why do I have to check the permissions of each range?
// why are the open and close syscalls in different header files
// can I assume that the size of the range will always be 4096? if so, I could save every other address and just specify size


// TODO:
// WEEK 1
// open proc/self/maps -- DONE
// read the file and parse it -- DONE
// print the address ranges -- DONE
// check the permissions of each range
// for each range, dump the memory it points to out to a file -- IN PROGRESS
// 12k range, 12k of data
// use open() and write() syscalls directly instead of fopen() and fwrite() -- DONE
// open using flag "wb" for write-binary -- UNECESSARY, using write instead of fwrite()

// WEEK 2
// array of struct -- no longer than 4096 BYTE using sizeof
// truncate system call -- use to make file bigger and zero it?
// seek to the end of the file and start writing there
// dump the memory of the current process to a file
// struct with long and struct in it
// next, create a tool that reads the file and sanity check the information
// using the header
// print out the address ranges

// WEEK 3
// change parser to only use one buffer (just null terminate each time)
// use constants for sizes of arrays, so that when you change one thing it changes both
// go through list of addresses and total virtual space
// truncate write file to 4096 + the total virtual space
// char buf[4096];
// myType *thing = (myType *) buf;
// use mmap to open the metadata file before reading proc/self/maps... then you don't have to write a copy buffer just write directly to the file


// WEEK 4
// double check that heap shows up in different location every time you run
// remove [vvar], [vdso], [stack], [syscall], keep [heap]
// add metadata
