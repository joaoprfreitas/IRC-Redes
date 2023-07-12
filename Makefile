client:
	rm -f client
	g++ client.cpp -lpthread -o client

server:
	rm -f server
	g++ server.cpp -lpthread -o server

all: client server
	./server