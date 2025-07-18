CC            := gcc
CFLAGS        := -g -fPIE -pie -I.
# this links server-side test binaries at a high address to avoid
# overlap when their image files are mapped
# into a client process. this should mirror the behaviour of
# position-independent code while ensuring the mapped
# regions will not conflict with the client code.
SERVER_LDFLAGS := -mcmodel=large -Wl,-Ttext-segment=0x10000000000
CLEAN_TARGETS := $(basename $(wildcard server_*.c) $(wildcard client_*.c) $(wildcard sbc_*.c))
LIBS          := $(wildcard *.a)
OBJECTS       := $(CLEAN_TARGETS:%=%.o)
TEST_BINS     := tests/server_test1 tests/server_test2 tests/server_test3 \
				 tests/client_test tests/seg_fault_test


# libraries
lib: libsbcserver.a libsbcclient.a

libsbcserver.a: sbc_server.o
	ar rcs libsbcserver.a sbc_server.o

libsbcclient.a: sbc_client.o sbc_mm.o
	ar rcs libsbcclient.a sbc_client.o sbc_mm.o


# object files
sbc_server.o: sbc_server.c vm_sbc.h
	$(CC) $(CFLAGS) -c sbc_server.c

sbc_client.o: sbc_client.c vm_sbc.h
	$(CC) $(CFLAGS) -c sbc_client.c

sbc_mm.o: sbc_mm.c vm_sbc.h
	$(CC) $(CFLAGS) -c sbc_mm.c


# tests
tests: $(TEST_BINS)

tests/client_test: tests/client_test.c libsbcclient.a
	$(CC) $(CFLAGS) $(LDFLAGS) $< -L . -l sbcclient -o $@

tests/seg_fault_test: tests/seg_fault_test.c libsbcclient.a libsbcserver.a
	$(CC) $(CFLAGS) $(LDFLAGS) $< -L . -l sbcclient -l sbcserver -o $@

tests/server_test1: tests/server_test1.c libsbcserver.a
	$(CC) $(CFLAGS) $(LDFLAGS) -mcmodel=large -Wl,-Ttext-segment=0x10100000000 $< -L . -l sbcserver -o $@

tests/server_test2: tests/server_test2.c libsbcserver.a
	$(CC) $(CFLAGS) $(LDFLAGS) -mcmodel=large -Wl,-Ttext-segment=0x10200000000 $< -L . -l sbcserver -o $@

tests/server_test3: tests/server_test3.c libsbcserver.a
	$(CC) $(CFLAGS) $(LDFLAGS) -mcmodel=large -Wl,-Ttext-segment=0x10300000000 $< -L . -l sbcserver -o $@


# if a file "clean" exists, ignore it and execute the below rule
.PHONY: clean tests run_tests

run_tests: tests
	cd tests && ./server_test1
	cd tests && ./server_test2
	cd tests && ./server_test3
	cd tests && ./client_test img_files/*.img || true
	cd tests && ./seg_fault_test || true
clean:
	rm -f $(LIBS) $(CLEAN_TARGETS) $(OBJECTS) $(TEST_BINS) img_files/* tests/img_files/*.img
