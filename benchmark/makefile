CC         = gcc
RM         = rm
CFLAGS     = -I. -Wall -g -O3 -I../sources
LFLAGS_LIB = ../sources/libmachinarium.a -pthread -lssl -lcrypto
LFLAGS     = $(LFLAGS_LIB)
EXAMPLES   = benchmark_csw benchmark_channel benchmark_queue
all: clean $(EXAMPLES)
benchmark_csw:
	$(CC) $(CFLAGS) benchmark_csw.c $(LFLAGS) -o benchmark_csw
benchmark_channel:
	$(CC) $(CFLAGS) benchmark_channel.c $(LFLAGS) -o benchmark_channel
benchmark_queue:
	$(CC) $(CFLAGS) benchmark_queue.c $(LFLAGS) -o benchmark_queue
clean:
	$(RM) -f $(EXAMPLES)
