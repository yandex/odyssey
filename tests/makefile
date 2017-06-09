
CC         = gcc
RM         = rm
CFLAGS     = -I. -Wall -g -O0 -I../src
LFLAGS_LIB = ../src/libmachinarium.a -pthread -lssl -lcrypto
LFLAGS     = $(LFLAGS_LIB)
OBJECTS    = machinarium_test.o \
             test_init.o \
             test_create0.o \
             test_create1.o \
             test_context_switch.o \
             test_sleep.o \
             test_sleep_yield.o \
             test_sleep_cancel0.o \
             test_join.o \
             test_condition0.o \
             test_condition1.o \
             test_channel_create.o \
             test_channel_rw0.o \
             test_channel_rw1.o \
             test_channel_rw2.o \
             test_channel_rw3.o \
             test_channel_rw4.o \
             test_channel_timeout.o \
             test_channel_cancel.o \
             test_queue_create.o \
             test_queue_rw0.o \
             test_queue_rw1.o \
             test_queue_rw2.o \
             test_queue_producer_consumer0.o \
             test_queue_producer_consumer1.o \
             test_io_new.o \
             test_connect.o \
             test_connect_timeout.o \
             test_connect_cancel0.o \
             test_connect_cancel1.o \
             test_accept_timeout.o \
             test_accept_cancel.o \
             test_getaddrinfo0.o \
             test_getaddrinfo1.o \
             test_getaddrinfo2.o \
             test_client_server0.o \
             test_client_server1.o \
             test_read_10mb.o \
             test_read_timeout.o \
             test_read_cancel.o \
             test_tls0.o \
             test_tls_read_10mb.o
all: clean $(OBJECTS) machinarium_test
machinarium_test:
	$(CC) $(OBJECTS) $(LFLAGS) -o machinarium_test
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f $(OBJECTS) machinarium_test
