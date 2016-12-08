# libmachinarium makefile.
#
CC      = gcc
AR      = ar
RM      = rm
CFLAGS  = -I. -Wall -g -O0 -pedantic
OBJECTS = mm_context.o \
          mm_fiber.o \
          mm_scheduler.o \
          mm.o \
          mm_io.o \
          mm_getsockname.o \
          mm_getpeername.o \
          mm_getaddrinfo.o \
          mm_connect.o \
          mm_bind.o \
          mm_accept.o \
          mm_read.o \
          mm_write.o
LIB     = libmachinarium.a
$(LIB): clean $(OBJECTS)
	$(AR) cr $(LIB) $(OBJECTS)
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f $(OBJECTS) $(LIB)
