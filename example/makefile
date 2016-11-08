CC      = gcc
RM      = rm
CFLAGS  = -I. -Wall -g -O0 -I../lib
LFLAGS  = ../lib/libfluent.a -luv
all: validate clean benchmark 
benchmark: benchmark.o
	$(CC) benchmark.o $(LFLAGS) -o benchmark
validate:
	@if [ ! -f ../lib/libfluent.a ]; then \
		echo ""; \
		echo "Please build the library first."; \
		echo ""; \
		exit 1; \
	fi
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f benchmark benchmark.o
