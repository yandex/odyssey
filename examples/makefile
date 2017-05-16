CC         = gcc
RM         = rm
CFLAGS     = -I. -Wall -g -O3 -I../src
LFLAGS_LIB = ../src/libmachinarium.a -pthread -lssl -lcrypto
LFLAGS     = $(LFLAGS_LIB)
EXAMPLES   = benchmark
all: clean $(EXAMPLES)
benchmark:
	$(CC) $(CFLAGS) benchmark.c $(LFLAGS) -o benchmark
clean:
	$(RM) -f $(EXAMPLES)
