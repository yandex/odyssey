CC         = gcc
RM         = rm
CFLAGS     = -I. -Wall -g -O0 -I../src
LFLAGS_LIB = ../src/libmachinarium.a -pthread -lssl -lcrypto
LFLAGS     = $(LFLAGS_LIB)
TESTS      = test_new \
             test_create \
             test_sleep \
             test_wait \
             test_condition \
             test_condition_2 \
             benchmark \
             test_io_new \
             test_connect \
             test_connect_timeout \
             test_cancel_connect \
             test_cancel_connect_2 \
             test_write \
             test_read \
             test_read_timeout \
             test_accept_timeout \
             test_cancel_accept \
             test_client_server \
             test_client_server_ra

#            test_getaddrinfo \
             test_getaddrinfo_2 \
             test_getaddrinfo_3 \
             test_cancel_sleep \
             test_cancel_sleep_2 \
             test_cancel_getaddrinfo \
             test_cancel_getaddrinfo_2 \
             test_cancel_read \
             test_tls_connect \
             echo
all: validate clean $(TESTS)
test_new:
	$(CC) $(CFLAGS) test_new.c $(LFLAGS) -o test_new
test_create:
	$(CC) $(CFLAGS) test_create.c $(LFLAGS) -o test_create
test_sleep:
	$(CC) $(CFLAGS) test_sleep.c $(LFLAGS) -o test_sleep
test_wait:
	$(CC) $(CFLAGS) test_wait.c $(LFLAGS) -o test_wait
test_condition:
	$(CC) $(CFLAGS) test_condition.c $(LFLAGS) -o test_condition
test_condition_2:
	$(CC) $(CFLAGS) test_condition_2.c $(LFLAGS) -o test_condition_2
test_io_new:
	$(CC) $(CFLAGS) test_io_new.c $(LFLAGS) -o test_io_new
test_getaddrinfo:
	$(CC) $(CFLAGS) test_getaddrinfo.c $(LFLAGS) -o test_getaddrinfo
test_getaddrinfo_2:
	$(CC) $(CFLAGS) test_getaddrinfo_2.c $(LFLAGS) -o test_getaddrinfo_2
test_getaddrinfo_3:
	$(CC) $(CFLAGS) test_getaddrinfo_3.c $(LFLAGS) -o test_getaddrinfo_3
test_connect:
	$(CC) $(CFLAGS) test_connect.c $(LFLAGS) -o test_connect
test_write:
	$(CC) $(CFLAGS) test_write.c $(LFLAGS) -o test_write
test_read:
	$(CC) $(CFLAGS) test_read.c $(LFLAGS) -o test_read
test_read_timeout:
	$(CC) $(CFLAGS) test_read_timeout.c $(LFLAGS) -o test_read_timeout
test_accept_timeout:
	$(CC) $(CFLAGS) test_accept_timeout.c $(LFLAGS) -o test_accept_timeout
test_connect_timeout:
	$(CC) $(CFLAGS) test_connect_timeout.c $(LFLAGS) -o test_connect_timeout
test_client_server:
	$(CC) $(CFLAGS) test_client_server.c $(LFLAGS) -o test_client_server
test_client_server_ra:
	$(CC) $(CFLAGS) test_client_server_ra.c $(LFLAGS) -o test_client_server_ra
test_cancel_sleep:
	$(CC) $(CFLAGS) test_cancel_sleep.c $(LFLAGS) -o test_cancel_sleep
test_cancel_sleep_2:
	$(CC) $(CFLAGS) test_cancel_sleep_2.c $(LFLAGS) -o test_cancel_sleep_2
test_cancel_getaddrinfo:
	$(CC) $(CFLAGS) test_cancel_getaddrinfo.c $(LFLAGS) -o test_cancel_getaddrinfo
test_cancel_getaddrinfo_2:
	$(CC) $(CFLAGS) test_cancel_getaddrinfo_2.c $(LFLAGS) -o test_cancel_getaddrinfo_2
test_cancel_connect:
	$(CC) $(CFLAGS) test_cancel_connect.c $(LFLAGS) -o test_cancel_connect
test_cancel_connect_2:
	$(CC) $(CFLAGS) test_cancel_connect_2.c $(LFLAGS) -o test_cancel_connect_2
test_cancel_read:
	$(CC) $(CFLAGS) test_cancel_read.c $(LFLAGS) -o test_cancel_read
test_cancel_accept:
	$(CC) $(CFLAGS) test_cancel_accept.c $(LFLAGS) -o test_cancel_accept
test_tls_connect:
	$(CC) $(CFLAGS) test_tls_connect.c $(LFLAGS) -o test_tls_connect
echo:
	$(CC) $(CFLAGS) echo.c $(LFLAGS) -o echo
benchmark:
	$(CC) $(CFLAGS) benchmark.c $(LFLAGS) -o benchmark
validate:
	@if [ ! -f ../src/libmachinarium.a ]; then \
		echo ""; \
		echo "Please build the static library first."; \
		echo ""; \
		exit 1; \
	fi
clean:
	$(RM) -f $(TESTS)
