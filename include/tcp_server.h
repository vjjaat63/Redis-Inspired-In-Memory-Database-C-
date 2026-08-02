#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "redis_clone.h"
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef SOCKET socket_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    typedef int socket_t;
#endif

namespace redis_clone {

class TCPServer {
public:
    using ClientHandler = std::function<void(socket_t client_socket, const std::string& client_address)>;

    TCPServer(int port);
    ~TCPServer();

    bool start();
    void stop();
    void set_client_handler(ClientHandler handler);

private:
    int port_;
    socket_t server_socket_;
    bool running_;
    std::thread accept_thread_;
    std::vector<std::thread> client_threads_;
    ClientHandler client_handler_;

    void accept_connections();
    void handle_client(socket_t client_socket, const std::string& client_address);
    
    bool initialize_socket();
    void cleanup_socket();
    std::string get_client_address(socket_t client_socket);
};

} // namespace redis_clone

#endif // TCP_SERVER_H
