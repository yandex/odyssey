CC      = gcc
RM      = rm
CFLAGS  = -I. -Wall -g -O0 -I../lib
LFLAGS  = ../lib/libflint.a -luv
all: validate clean benchmark echo wait cancel cancel_connect
benchmark: benchmark.o
	$(CC) benchmark.o $(LFLAGS) -o benchmark
echo: echo.o
	$(CC) echo.o $(LFLAGS) -o echo
wait: wait.o
	$(CC) wait.o $(LFLAGS) -o wait
cancel: cancel.o
	$(CC) cancel.o $(LFLAGS) -o cancel
cancel_connect: cancel_connect.o
	$(CC) cancel_connect.o $(LFLAGS) -o cancel_connect
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
	$(RM) -f wait wait.o
	$(RM) -f cancel cancel.o
	$(RM) -f cancel_connect cancel_connect.o
