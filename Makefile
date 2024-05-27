all:
	g++ -w console.cpp -o hw4.cgi -std=c++14 -Wall -pedantic -pthread -lboost_system
	g++ -w socks_server.cpp -o socks_server -std=c++14 -Wall -pedantic -pthread -lboost_system
clean:
	rm -f hw4.cgi socks_server