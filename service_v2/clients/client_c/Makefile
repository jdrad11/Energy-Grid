CC = gcc

all: test-client

test-client: test-client.o grid-client.o
	$(CC) -lcrypto -o $@ $^

test-client.o: test-client.c
	$(CC) -c $<

grid-client.o: grid-client.c
	$(CC) -c $<

clean:
	rm -f *.o test-client

.PHONY: all clean
