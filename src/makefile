# odissey makefile.
#
CC      = gcc
RM      = rm
CFLAGS  = -I. -Wall -O0 -g -pedantic
LFLAGS  = 
OBJECTS = od_log.o od_lex.o od_scheme.o od_config.o od.o odissey.o
TARGET  = odissey
$(TARGET): clean $(OBJECTS)
	$(CC) $(OBJECTS) $(LFLAGS) -o $(TARGET)
.c.o:
	$(CC) $(CFLAGS) -c $<
clean:
	$(RM) -f $(OBJECTS) $(TARGET)
