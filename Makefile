RM = rm
CC = gcc
# CFLAGS = -ggdb -Wall -pedantic -std=c99
CFLAGS = -ggdb -Wall
LIBS = -lbluetooth -lcurl -lm

MAIN = smatool
MAIN_OBJS = hexdump.o sunlight.o smatool.o logging.o bluetooth.o

TEST = db_test
TEST_OBJ = db_test.o

SQLITE_LIB = -lsqlite3
SQLITE_OBJ = db_sqlite3.o

MYSQL_LIB = -lmysqlclient
MYSQL_OBJ = db_mysql.o

HEADER=pvlogger.h logging.h

$(MAIN) : $(MYSQL_OBJ) $(MAIN_OBJS)
	$(CC) $(LIBS) $(MYSQL_LIB) $(CFLAGS) -o $(MAIN) $(MAIN_OBJS) $(MYSQL_OBJ)

.c.o :
	$(CC) $(CFLAGS) -c $<  -o $@

.PHONY: clean
clean:
	$(RM) *.o  $(MAIN) $(TEST)

sqlite : $(SQLITE_OBJ) $(MAIN_OBJS)
	$(CC) $(CFLAGS) -o $(MAIN) $(MAIN_OBJS) $(SQLITE_OBJ) $(LIBS) $(SQLITE_LIB)

test : $(MYSQL_OBJ) $(TEST_OBJ)
	$(CC) $(LIBS) $(MYSQL_LIB) $(CFLAGS) -o $(TEST) $(MYSQL_OBJ) $(TEST_OBJ)

sqlite_test : $(SQLITE_OBJ) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $(TEST) $(SQLITE_OBJ) $(TEST_OBJ) $(LIBS) $(SQLITE_LIB)
