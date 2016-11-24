CC         = gcc
RM         = rm
CFLAGS     = -I. -Wall -g -O0 -I../machinarium
LFLAGS_LIB = ../machinarium/libmachinarium.a
LFLAGS     = $(LFLAGS_LIB) -luv
all: validate clean env
env:
	$(CC) $(CFLAGS) env.c $(LFLAGS) -o env
validate:
	@if [ ! -f $(LFLAGS_LIB) ]; then \
		echo ""; \
		echo "Please build the library first."; \
		echo ""; \
		exit 1; \
	fi
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f env
