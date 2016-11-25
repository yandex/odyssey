CC         = gcc
RM         = rm
CFLAGS     = -I. -Wall -g -O0 -I../machinarium
LFLAGS_LIB = ../machinarium/libmachinarium.a
LFLAGS_LIB_UV = ~/temp/libuv/.libs/libuv.a -pthread
LFLAGS     = $(LFLAGS_LIB) $(LFLAGS_LIB_UV)
TESTS      = test_new \
             test_create \
             test_sleep \
             test_wait \
             test_cancel_sleep \
             test_cancel_sleep_2 \
             test_io_new \
             test_io_connect
all: validate clean $(TESTS)
test_new:
	$(CC) $(CFLAGS) test_new.c $(LFLAGS) -o test_new
test_create:
	$(CC) $(CFLAGS) test_create.c $(LFLAGS) -o test_create
test_sleep:
	$(CC) $(CFLAGS) test_sleep.c $(LFLAGS) -o test_sleep
test_wait:
	$(CC) $(CFLAGS) test_wait.c $(LFLAGS) -o test_wait
test_cancel_sleep:
	$(CC) $(CFLAGS) test_cancel_sleep.c $(LFLAGS) -o test_cancel_sleep
test_cancel_sleep_2:
	$(CC) $(CFLAGS) test_cancel_sleep_2.c $(LFLAGS) -o test_cancel_sleep_2
test_io_new:
	$(CC) $(CFLAGS) test_io_new.c $(LFLAGS) -o test_io_new
test_io_connect:
	$(CC) $(CFLAGS) test_io_connect.c $(LFLAGS) -o test_io_connect
validate:
	@if [ ! -f $(LFLAGS_LIB) ]; then \
		echo ""; \
		echo "Please build the library first."; \
		echo ""; \
		exit 1; \
	fi
clean:
	$(RM) -f $(TESTS)
