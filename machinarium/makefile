# libmachinarium makefile.
#
CC      = gcc
AR      = ar
RM      = rm
CFLAGS  = -I. -Wall -g -O0 -pedantic
OBJECTS = mm_context.o \
          mm_fiber.o
LIB     = libmachinarium.a
$(LIB): clean $(OBJECTS)
	$(AR) cr $(LIB) $(OBJECTS)
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f $(OBJECTS) $(LIB)
