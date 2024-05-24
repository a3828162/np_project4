all:
	g++ -w http_server.cpp -o http_server -std=c++14 -Wall -pedantic -pthread -lboost_system
	g++ -w console.cpp -o hw4.cgi -std=c++14 -Wall -pedantic -pthread -lboost_system
	g++ -w socks_server.cpp -o socks_server -std=c++14 -Wall -pedantic -pthread -lboost_system
1:
	g++ -w socks_server.cpp -o socks_server -std=c++14 -Wall -pedantic -pthread -lboost_system
clean:
	rm -f http_server console.cgi socks_server