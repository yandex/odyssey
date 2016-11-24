CC         = gcc
RM         = rm
CFLAGS     = -I. -Wall -g -O0 -I../machinarium
LFLAGS_LIB = ../machinarium/libmachinarium.a
LFLAGS_LIB_UV = ~/temp/libuv/.libs/libuv.a -pthread
LFLAGS     = $(LFLAGS_LIB) $(LFLAGS_LIB_UV)
TESTS      = new create sleep wait cancel_sleep cancel_sleep_2 io_new
all: validate clean $(TESTS)
new:
	$(CC) $(CFLAGS) new.c $(LFLAGS) -o new
create:
	$(CC) $(CFLAGS) create.c $(LFLAGS) -o create
sleep:
	$(CC) $(CFLAGS) sleep.c $(LFLAGS) -o sleep
wait:
	$(CC) $(CFLAGS) wait.c $(LFLAGS) -o wait
cancel_sleep:
	$(CC) $(CFLAGS) cancel_sleep.c $(LFLAGS) -o cancel_sleep
cancel_sleep_2:
	$(CC) $(CFLAGS) cancel_sleep_2.c $(LFLAGS) -o cancel_sleep_2
io_new:
	$(CC) $(CFLAGS) io_new.c $(LFLAGS) -o io_new
validate:
	@if [ ! -f $(LFLAGS_LIB) ]; then \
		echo ""; \
		echo "Please build the library first."; \
		echo ""; \
		exit 1; \
	fi
clean:
	$(RM) -f $(TESTS)
