# libfluent makefile.
#
CC      = gcc
AR      = ar
RM      = rm
CFLAGS  = -I. -Wall -g -O0 -pedantic
OBJECTS =
LIB     = libfluent.a
$(LIB): clean $(OBJECTS)
	$(AR) cr $(LIB) $(OBJECTS)
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f $(OBJECTS) $(LIB)
