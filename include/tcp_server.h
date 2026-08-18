#pragma once

#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

using namespace std;

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
    typedef int socket_t;
#endif

namespace redis_clone {

class TCPServer {
public:
    using MessageHandler = function<string(const string& request, size_t& consumed)>;

    explicit TCPServer(int port);
    ~TCPServer();

    bool start();
    void stop();
    void set_handler(MessageHandler handler);

private:
    int port_;
    socket_t server_socket_;
    atomic<bool> running_{false};
    thread accept_thread_;
    vector<thread> client_threads_;
    MessageHandler handler_;

    void accept_connections();
    void handle_client(socket_t client_socket);
    bool init_socket();
    void close_socket();
};

} // namespace redis_clone
