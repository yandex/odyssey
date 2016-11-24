CC         = gcc
RM         = rm
CFLAGS     = -I. -Wall -g -O0 -I../machinarium
LFLAGS_LIB = ../machinarium/libmachinarium.a
LFLAGS_LIB_UV = ~/temp/libuv/.libs/libuv.a -pthread
LFLAGS     = $(LFLAGS_LIB) $(LFLAGS_LIB_UV)
TESTS      = env create wait
all: validate clean $(TESTS)
env:
	$(CC) $(CFLAGS) env.c $(LFLAGS) -o env
create:
	$(CC) $(CFLAGS) create.c $(LFLAGS) -o create
wait:
	$(CC) $(CFLAGS) wait.c $(LFLAGS) -o wait
validate:
	@if [ ! -f $(LFLAGS_LIB) ]; then \
		echo ""; \
		echo "Please build the library first."; \
		echo ""; \
		exit 1; \
	fi
clean:
	$(RM) -f $(TESTS)
