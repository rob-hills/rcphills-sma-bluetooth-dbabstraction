RM = rm
CC = gcc
CFLAGS = -g -Wall
LIBS = -lbluetooth -lcurl -lmysqlclient

MAIN = smatool

OBJECTS = smatool.o

$(MAIN) : $(OBJECTS)
	$(CC) $(LIBS) $(CFLAGS) -o $(MAIN) $(OBJECTS)

.c.o :
	$(CC) $(CFLAGS) -c $<  -o $@

.PHONY: clean
clean:
	$(RM) *.o  $(MAIN) 

