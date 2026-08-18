#include "tcp_server.h"
#include <iostream>
#include <cstring>

using namespace std;

#ifndef _WIN32
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

namespace redis_clone {

TCPServer::TCPServer(int port)
    : port_(port), server_socket_(INVALID_SOCKET) {
}

TCPServer::~TCPServer() {
    stop();
}

bool TCPServer::init_socket() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
#endif

    server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket_ == INVALID_SOCKET) return false;

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(server_socket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        close_socket();
        return false;
    }

    if (listen(server_socket_, SOMAXCONN) == SOCKET_ERROR) {
        close_socket();
        return false;
    }

    return true;
}

void TCPServer::close_socket() {
    if (server_socket_ != INVALID_SOCKET) {
        closesocket(server_socket_);
        server_socket_ = INVALID_SOCKET;
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

bool TCPServer::start() {
    if (!init_socket()) {
        cerr << "[ERROR] Failed to bind/listen on port " << port_ << endl;
        return false;
    }

    running_ = true;
    accept_thread_ = thread(&TCPServer::accept_connections, this);
    cout << "[INFO] Redis Server listening on port " << port_ << endl;
    return true;
}

void TCPServer::stop() {
    if (!running_) return;
    running_ = false;

    close_socket();

    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }

    for (auto& t : client_threads_) {
        if (t.joinable()) t.join();
    }
    client_threads_.clear();
}

void TCPServer::set_handler(MessageHandler handler) {
    handler_ = move(handler);
}

void TCPServer::accept_connections() {
    while (running_) {
        sockaddr_in client_addr{};
#ifdef _WIN32
        int addr_len = sizeof(client_addr);
#else
        socklen_t addr_len = sizeof(client_addr);
#endif

        socket_t client = accept(server_socket_, (sockaddr*)&client_addr, &addr_len);
        if (client == INVALID_SOCKET) continue;

        client_threads_.emplace_back([this, client]() {
            handle_client(client);
        });
    }
}

void TCPServer::handle_client(socket_t client_socket) {
    char buffer[4096];
    string stream_buffer;

    while (running_) {
#ifdef _WIN32
        int bytes = recv(client_socket, buffer, sizeof(buffer), 0);
#else
        ssize_t bytes = recv(client_socket, buffer, sizeof(buffer), 0);
#endif
        if (bytes <= 0) break;

        stream_buffer.append(buffer, bytes);

        while (!stream_buffer.empty()) {
            size_t consumed = 0;
            string response;
            if (handler_) {
                response = handler_(stream_buffer, consumed);
            }
            if (consumed == 0) break; // Need more data from socket

#ifdef _WIN32
            send(client_socket, response.c_str(), static_cast<int>(response.length()), 0);
#else
            send(client_socket, response.c_str(), response.length(), 0);
#endif
            stream_buffer.erase(0, consumed);
        }
    }

    closesocket(client_socket);
}

} // namespace redis_clone
