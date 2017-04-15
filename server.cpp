#include "rdma_socket.hpp"

int main(int argc, char* argv[]) {
    rdma_socket server(RDMA_PS_TCP);

    server.bind("192.168.3.3", 8000);
    server.listen();
    rdma_socket rclient = server.accept();

    char* data = new char[4096];
    std::memcpy(data, "Hello World!", 13); 

    rclient.post_send(data, 4096);

    struct ibv_wc wc = rclient.send_comp();
    return 0;
}

