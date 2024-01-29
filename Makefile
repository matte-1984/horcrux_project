all:
	g++ ./server/server.cpp -std=c++20 -o ./server/horcrux_server -lboost_system
	g++ ./client/client.cpp -std=c++20 -o ./client/horcrux -lboost_system