# libflint makefile.
#
CC      = gcc
AR      = ar
RM      = rm
CFLAGS  = -I. -Wall -g -O0 -pedantic
OBJECTS = ft_context.o \
          ft_fiber.o \
          ft_scheduler.o \
          ft.o \
          ft_io.o \
          ft_ioconnect.o \
          ft_iobind.o \
          ft_ioaccept.o \
          ft_ioread.o \
          ft_iowrite.o

LIB     = libflint.a
$(LIB): clean $(OBJECTS)
	$(AR) cr $(LIB) $(OBJECTS)
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f $(OBJECTS) $(LIB)
