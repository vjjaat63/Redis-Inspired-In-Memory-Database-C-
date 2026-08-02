#include "tcp_server.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

namespace redis_clone {

TCPServer::TCPServer(int port)
    : port_(port), server_socket_(INVALID_SOCKET), running_(false) {
}

TCPServer::~TCPServer() {
    stop();
}

bool TCPServer::initialize_socket() {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        std::cerr << "WSAStartup failed" << std::endl;
        return false;
    }
#endif

    server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket_ == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // Set socket options to reuse address
    int opt = 1;
#ifdef _WIN32
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)) == SOCKET_ERROR) {
#else
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == SOCKET_ERROR) {
#endif
        std::cerr << "Failed to set socket options" << std::endl;
        cleanup_socket();
        return false;
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Failed to bind socket to port " << port_ << std::endl;
        cleanup_socket();
        return false;
    }

    if (listen(server_socket_, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Failed to listen on socket" << std::endl;
        cleanup_socket();
        return false;
    }

    return true;
}

void TCPServer::cleanup_socket() {
    if (server_socket_ != INVALID_SOCKET) {
        closesocket(server_socket_);
        server_socket_ = INVALID_SOCKET;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

bool TCPServer::start() {
    if (!initialize_socket()) {
        return false;
    }

    running_ = true;
    accept_thread_ = std::thread(&TCPServer::accept_connections, this);
    
    std::cout << "TCP Server started on port " << port_ << std::endl;
    return true;
}

void TCPServer::stop() {
    running_ = false;
    
    // Close server socket to unblock accept()
    if (server_socket_ != INVALID_SOCKET) {
        closesocket(server_socket_);
        server_socket_ = INVALID_SOCKET;
    }

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    // Wait for all client threads to finish
    for (auto& thread : client_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    client_threads_.clear();

    cleanup_socket();
    std::cout << "TCP Server stopped" << std::endl;
}

void TCPServer::set_client_handler(ClientHandler handler) {
    client_handler_ = handler;
}

void TCPServer::accept_connections() {
    while (running_) {
        sockaddr_in client_addr;
#ifdef _WIN32
        int client_addr_len = sizeof(client_addr);
#else
        socklen_t client_addr_len = sizeof(client_addr);
#endif

        socket_t client_socket = accept(server_socket_, (sockaddr*)&client_addr, &client_addr_len);
        
        if (client_socket == INVALID_SOCKET) {
            if (running_) {
                std::cerr << "Failed to accept client connection" << std::endl;
            }
            continue;
        }

        std::string client_address = get_client_address(client_socket);
        std::cout << "Client connected: " << client_address << std::endl;

        // Handle client in a new thread
        client_threads_.emplace_back([this, client_socket, client_address]() {
            handle_client(client_socket, client_address);
        });
    }
}

void TCPServer::handle_client(socket_t client_socket, const std::string& client_address) {
    if (client_handler_) {
        client_handler_(client_socket, client_address);
    } else {
        // Default handler: echo back received data
        char buffer[4096];
        while (true) {
#ifdef _WIN32
            int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
#else
            ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
#endif

            if (bytes_received <= 0) {
                break;
            }

#ifdef _WIN32
            send(client_socket, buffer, bytes_received, 0);
#else
            send(client_socket, buffer, bytes_received, 0);
#endif
        }
    }

    closesocket(client_socket);
    std::cout << "Client disconnected: " << client_address << std::endl;
}

std::string TCPServer::get_client_address(socket_t client_socket) {
    sockaddr_in client_addr;
#ifdef _WIN32
    int addr_len = sizeof(client_addr);
#else
    socklen_t addr_len = sizeof(client_addr);
#endif

    if (getpeername(client_socket, (sockaddr*)&client_addr, &addr_len) == 0) {
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
        return std::string(ip_str) + ":" + std::to_string(ntohs(client_addr.sin_port));
    }

    return "unknown";
}

} // namespace redis_clone
