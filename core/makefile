# odissey makefile.
#
CC             = gcc
RM             = rm
FLINT_CFLAGS   = -I../lib/flint/lib
FLINT_LFLAGS   = ../lib/flint/lib/libflint.a -luv
SOPRANO_CFLAGS = -I../lib/soprano/lib
SOPRANO_LFLAGS = ../lib/soprano/lib/libsoprano.a
CFLAGS         = -I. -Wall -O0 -g -pedantic $(FLINT_CFLAGS) $(SOPRANO_CFLAGS)
LFLAGS         = $(FLINT_LFLAGS) $(SOPRANO_LFLAGS)
OBJECTS        = od_log.o \
                 od_lex.o \
                 od_scheme.o \
                 od_config.o \
                 od.o \
                 od_io.o \
                 od_client_pool.o \
                 od_server_pool.o \
                 od_route_pool.o \
                 od_pooler.o \
                 od_router.o \
                 od_fe.o \
                 od_be.o \
                 odissey.o
TARGET  = odissey
$(TARGET): clean $(OBJECTS)
	$(CC) $(OBJECTS) $(LFLAGS) -o $(TARGET)
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f $(OBJECTS) $(TARGET)
