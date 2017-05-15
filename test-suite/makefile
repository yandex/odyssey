
CC         = gcc
RM         = rm
CFLAGS     = -I. -Wall -g -O0 -I../src
LFLAGS_LIB = ../src/libmachinarium.a -pthread -lssl -lcrypto
LFLAGS     = $(LFLAGS_LIB)
OBJECTS    = machinarium_test.o \
             test_init.o \
             test_create.o \
             test_sleep.o \
             test_sleep_yield.o \
             test_sleep_cancel0.o \
             test_sleep_cancel1.o
all: clean $(OBJECTS) machinarium_test
machinarium_test:
	$(CC) $(OBJECTS) $(LFLAGS) -o machinarium_test
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f $(OBJECTS) machinarium_test
