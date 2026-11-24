CC      = gcc
CFLAGS  = -Wall -pthread
AR      = ar
LIB     = libfactorial.a
LIB_OBJ = factorial.o netio.o

.PHONY: all clean

all: client server

# статическая библиотека с общим кодом клиента и сервера
$(LIB): $(LIB_OBJ)
	$(AR) rcs $@ $^

client: client.o $(LIB)
	$(CC) $(CFLAGS) -o $@ client.o -L. -lfactorial

server: server.o $(LIB)
	$(CC) $(CFLAGS) -o $@ server.o -L. -lfactorial

%.o: %.c factorial.h netio.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(LIB) client server