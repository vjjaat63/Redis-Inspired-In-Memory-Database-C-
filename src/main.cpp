#include "data_store.h"
#include "resp_parser.h"
#include "command_executor.h"
#include "tcp_server.h"
#include "persistence.h"
#include <iostream>
#include <csignal>
#include <memory>
#include <chrono>
#include <thread>
#include <atomic>

using namespace std;
using namespace redis_clone;

static atomic<bool> g_running{true};
static unique_ptr<TCPServer> g_server;

void signal_handler(int signal) {
    cout << "\n[INFO] Shutting down Redis Server (Signal " << signal << ")..." << endl;
    g_running = false;
    if (g_server) g_server->stop();
    exit(0);
}

int main(int argc, char* argv[]) {
    int port = 6379;
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = atoi(argv[++i]);
        }
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    cout << "===========================================" << endl;
    cout << "  Redis Clone (C++ In-Memory DB Engine)    " << endl;
    cout << "===========================================" << endl;

    auto store = make_shared<DataStore>();
    auto executor = make_shared<CommandExecutor>(store);
    auto persistence = make_shared<Persistence>(store);

    // Auto-restore snapshot if available
    if (persistence->load_snapshot("dump.rdb")) {
        cout << "[INFO] Loaded existing snapshot from dump.rdb (" << store->dbsize() << " keys restored)" << endl;
    }

    g_server = make_unique<TCPServer>(port);
    g_server->set_handler([executor](const string& request, size_t& consumed) -> string {
        vector<string> args;
        if (!RESPParser::parse_command(request, args, consumed) || consumed == 0) return "";
        return executor->execute(args);
    });

    if (!g_server->start()) {
        cerr << "[FATAL] Failed to start server on port " << port << endl;
        return 1;
    }

    // Periodic snapshot & TTL cleanup thread
    thread background_worker([store, persistence]() {
        while (g_running) {
            this_thread::sleep_for(chrono::seconds(1));
            store->cleanup_expired_keys();
        }
    });

    cout << "[INFO] Server ready to accept client connections. Press Ctrl+C to stop." << endl;

    while (g_running) {
        this_thread::sleep_for(chrono::seconds(1));
    }

    if (background_worker.joinable()) background_worker.join();
    return 0;
}
