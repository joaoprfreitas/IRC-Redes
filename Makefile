all: clean
	g++ server.cpp -lpthread -o server
	g++ client.cpp -lpthread -o client

clean:
	rm -f client server
