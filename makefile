CXX=g++ -std=c++11
LIBS=-lrdmacm -libverbs

all: client server

client: client.cpp rdma_socket.hpp
	$(CXX) -o client client.cpp $(LIBS)

server: server.cpp rdma_socket.hpp
	$(CXX) -o server server.cpp $(LIBS)
