CC      = gcc
RM      = rm
CFLAGS  = -I. -Wall -g -O0 -I../lib
LFLAGS  = ../lib/libflint.a -luv
all: validate clean benchmark echo
benchmark: benchmark.o
	$(CC) benchmark.o $(LFLAGS) -o benchmark
echo: echo.o
	$(CC) echo.o $(LFLAGS) -o echo
validate:
	@if [ ! -f ../lib/libflint.a ]; then \
		echo ""; \
		echo "Please build the library first."; \
		echo ""; \
		exit 1; \
	fi
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f benchmark benchmark.o
	$(RM) -f echo echo.o
