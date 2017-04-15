#include "rdma_socket.hpp"

int main(int argc, char* argv[]) {
    rdma_socket client(RDMA_PS_TCP);

    client.resolve("192.168.3.3", 8000);
    client.create_qp();
    
    char* mem = new char[4096];    
    client.post_recv(mem, 4096); 

    client.connect();
 
    struct ibv_wc wc = client.recv_comp();
    printf("%s\n", reinterpret_cast<char*>(wc.wr_id));
    return 0;
}
