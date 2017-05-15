
CC         = gcc
RM         = rm
CFLAGS     = -I. -Wall -g -O0 -I../src
LFLAGS_LIB = ../src/libmachinarium.a -pthread -lssl -lcrypto
LFLAGS     = $(LFLAGS_LIB)
OBJECTS    = machinarium_test.o \
             test_init.o \
             test_create.o \
             test_context_switch.o \
             test_sleep.o \
             test_sleep_yield.o \
             test_sleep_cancel0.o \
             test_sleep_cancel1.o \
             test_wait.o \
             test_condition0.o \
             test_condition1.o \
             test_io_new.o \
             test_connect.o \
             test_connect_timeout.o \
             test_connect_cancel0.o \
             test_connect_cancel1.o \
             test_accept_timeout.o \
             test_accept_cancel.o \
             test_client_server.o \
             test_client_server_readahead.o
all: clean $(OBJECTS) machinarium_test
machinarium_test:
	$(CC) $(OBJECTS) $(LFLAGS) -o machinarium_test
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f $(OBJECTS) machinarium_test
