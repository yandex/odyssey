# odissey makefile.
#
CC      = gcc
RM      = rm
CFLAGS  = -I. -Wall -O0 -g -pedantic -std=c99
LFLAGS  = 
OBJECTS = od_scheme.o odissey.o
TARGET  = odissey

$(TARGET): clean $(OBJECTS)
	$(CC) $(OBJECTS) $(LFLAGS) -o $(TARGET)
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f $(OBJECTS) $(TARGET)
