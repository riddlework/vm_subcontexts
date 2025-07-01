# CFLAGS=-g -fPIE -pie -Wl,-z,relro,-z,now -fno-plt
# CFLAGS=-g -Wl,-z,relro,-z,now
CC            := gcc
CFLAGS        := -g -fPIE -pie
CLEAN_TARGETS := $(basename $(wildcard server_*.c) $(wildcard client_*.c) $(wildcard sbc_*.c))
LIBS          := $(wildcard *.a)
OBJECTS       := $(CLEAN_TARGETS:%=%.o)

lib: libsbcserver.a libsbcclient.a

libsbcserver.a: sbc_server.o
	ar rcs libsbcserver.a sbc_server.o

libsbcclient.a: sbc_client.o
	ar rcs libsbcclient.a sbc_client.o

sbc_server.o: sbc_server.c vm_sbc.h
	$(CC) $(CFLAGS) -c sbc_server.c

sbc_client.o: sbc_client.c vm_sbc.h
	$(CC) $(CFLAGS) -c sbc_client.c

server_%: server_%.c libsbcserver.a
	$(CC) $(CFLAGS) $(LDFLAGS) server_$*.c -L . -l sbcserver -o server_$*

client_%: client_%.c libsbcclient.a
	$(CC) $(CFLAGS) $(LDFLAGS) client_$*.c -L . -l sbcclient -o client_$*

# if a file "clean" exists, ignore it and execute the below rule
.PHONY: clean

clean:
	rm -f $(LIBS) $(CLEAN_TARGETS) $(OBJECTS) img_files/*
