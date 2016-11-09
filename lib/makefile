# libsonata makefile
#
CC      = gcc
RM      = rm
CFLAGS  = -I. -Wall -g -O0 -pedantic
LFLAGS  = 
OBJECTS = so_read.o so_beread.o
LIB     = libsonata.a

all: clean $(LIB)
$(LIB): $(OBJECTS)
	$(AR) cr $(LIB) $(OBJECTS)
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f $(OBJECTS) $(TEST) $(LIB)
