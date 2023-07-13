all: clean
	g++ server.cpp -lpthread -o server
	g++ client.cpp -lpthread -o client

client: cleanclient
	g++ client.cpp -lpthread -o client

server:
	rm -f server
	g++ server.cpp -lpthread -o server

cleanclient:
	rm -f client

clean:
	rm -f client server
