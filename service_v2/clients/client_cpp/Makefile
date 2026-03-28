CXX = g++

all: test-client

test-client: test-client.o grid-client.o
	$(CXX) -lcrypto -o $@ $^

test-client.o: test-client.cpp
	$(CXX) -c $<

grid-client.o: grid-client.cpp
	$(CXX) -c $<

clean:
	rm -f *.o test-client

.PHONY: all clean
