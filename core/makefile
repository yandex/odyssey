# odissey makefile.
#
CC            = gcc
RM            = rm
FLINT_CFLAGS  = -I../lib/flint/lib
FLINT_LFLAGS  = ../lib/flint/lib/libflint.a -luv
CFLAGS        = -I. -Wall -O0 -g -pedantic $(FLINT_CFLAGS)
LFLAGS        = $(FLINT_LFLAGS)
OBJECTS       = od_log.o \
                od_lex.o \
                od_scheme.o \
                od_config.o \
                od.o \
                od_server_pool.o \
                od_client_pool.o \
                od_pooler.o \
                od_router.o \
                odissey.o
TARGET  = odissey
$(TARGET): clean $(OBJECTS)
	$(CC) $(OBJECTS) $(LFLAGS) -o $(TARGET)
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f $(OBJECTS) $(TARGET)
