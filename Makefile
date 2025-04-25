CC = gcc
# CFLAGS=-g -fPIE -pie -Wl,-z,relro,-z,now -fno-plt
CFLAGS=-g -Wl,-z,relro,-z,now
# CFLAGS=-g -fPIE -pie
# CFLAGS=-g -fno-plt -fPIE -Wl,-z,now
# CFLAGS=-g -fPIE
# LDFLAGS=-static-pie
# LDFLAGS=-pie

all: map unmap


# map.o: map.c
# 	$(cc) $(cflags) $(ldflags) -c map.c
#
# unmap.o: unmap.c
# 	$(CC) $(CFLAGS) $(LDFLAGS) -c unmap.c
# map: map.o
# 	$(cc) $(cflags) $(ldflags) -o map map.o
# unmap: unmap.o
# 	$(cc) $(cflags) $(ldflags) -o unmap unmap.o

clean:
	rm -f *.o map unmap mem reconstructed_maps

