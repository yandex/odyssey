CC         = gcc
RM         = rm
CFLAGS     = -I. -Wall -g -O0 -I../machinarium
LFLAGS_LIB = ../machinarium/libmachinarium.a
LFLAGS     = $(LFLAGS_LIB) -luv
TESTS      = env create
all: validate clean $(TESTS)
env:
	$(CC) $(CFLAGS) env.c $(LFLAGS) -o env
create:
	$(CC) $(CFLAGS) create.c $(LFLAGS) -o create
validate:
	@if [ ! -f $(LFLAGS_LIB) ]; then \
		echo ""; \
		echo "Please build the library first."; \
		echo ""; \
		exit 1; \
	fi
run:
	./env
	./create
clean:
	$(RM) -f $(TESTS)
