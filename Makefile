client:
	rm -f client
	g++ client.cpp -lpthread -o client

server:
	rm -f server
	g++ server.cpp -lpthread -o server

clean:
	rm -f client server

all: client server
	./server