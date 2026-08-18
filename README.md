# Redis Clone - C++ In-Memory Key-Value Database

A high-performance, multithreaded in-memory key-value database built in C++ with Redis-like data structures, RESP protocol support, and file-based snapshot persistence.

---

## 🚀 Key Highlights & Specifications

- **Tools & Technologies**: C++17, STL, TCP Sockets, Multithreading (`std::thread`), Concurrency & Mutexes (`std::shared_mutex`), RESP Protocol.
- **Multithreaded Network Server**: Engineered a client-server in-memory database using TCP sockets and mutex synchronization to safely handle concurrent client connections without race conditions.
- **RESP Protocol & Core Commands**: Full Redis Serialization Protocol (RESP) parser and serializer supporting sub-millisecond execution for **String**, **List**, and **Hash** data structures, along with **TTL / Expiration**.
- **Snapshot Persistence**: Periodic and on-demand file-based database snapshots for reliable crash recovery and state restoration.

---

## 🏛 Architecture

```
                       [ Client (e.g. redis-cli) ]
                                   │
                           Raw TCP Bytes
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 1. Network Layer: TCPServer (tcp_server.h / tcp_server.cpp)            │
│    - Multi-client connection handling                                  │
│    - Concurrent worker thread per connection                           │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                       RESP Stream ("*3\r\n$3\r\nSET...")
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 2. Protocol Layer: RESPParser (resp_parser.h / resp_parser.cpp)        │
│    - Deserializes raw stream into parsed command tokens                │
│    - Serializes output into RESP format ("+OK\r\n", ":100\r\n", etc.) │
└──────────────────────────────────┬─────────────────────────────────────┘
                                   │
                      Parsed Command: ["SET", "key", "val"]
                                   ▼
┌────────────────────────────────────────────────────────────────────────┐
│ 3. Execution Layer: CommandExecutor (command_executor.h / .cpp)        │
│    - Dispatches to handlers (SET, GET, LPUSH, HSET, EXPIRE, etc.)      │
└──────────────────┬─────────────────────────────────┬───────────────────┘
                   │                                 │
            Executes Operation                  Snapshot Save / Restore
                   ▼                                 ▼
┌──────────────────────────────────────┐  ┌──────────────────────────────┐
│ 4. Storage: DataStore                │  │ 5. Persistence:              │
│    (data_store.h / data_store.cpp)   │  │    (persistence.h / .cpp)    │
│    - std::unordered_map store_       │  │    - File-based RDB snapshot │
│    - std::shared_mutex concurrency   │  │    - Crash recovery loader   │
│    - TTL & key expiration            │  └──────────────────────────────┘
└──────────────────────────────────────┘
```

---

## 📂 Streamlined Project Structure

```
redis-clone/
├── README.md                   # Project overview & documentation
├── include/                    # Header files
│   ├── redis_clone.h          # Core types (RedisValue, KeyEntry, RedisType)
│   ├── data_store.h           # Thread-safe in-memory key-value store
│   ├── resp_parser.h          # RESP protocol serializer & parser
│   ├── command_executor.h     # Command routing & handler logic
│   ├── tcp_server.h           # Multithreaded cross-platform TCP server
│   └── persistence.h          # File snapshot persistence
└── src/                       # Implementation files
    ├── data_store.cpp         # Storage & TTL operations
    ├── resp_parser.cpp        # Protocol parsing & formatting
    ├── command_executor.cpp   # Command execution
    ├── tcp_server.cpp         # Socket server & thread management
    ├── persistence.cpp        # Disk snapshot save/load
    ├── main.cpp               # Server entry point
    └── test_main.cpp          # Automated verification test suite
```

---

## 🛠 Supported Commands

| Category | Commands |
| :--- | :--- |
| **Strings** | `SET`, `GET`, `DEL`, `EXISTS`, `KEYS` |
| **Lists** | `LPUSH`, `RPUSH`, `LPOP`, `RPOP`, `LRANGE`, `LLEN` |
| **Hashes** | `HSET`, `HGET`, `HDEL`, `HGETALL`, `HLEN` |
| **Expiration** | `EXPIRE`, `TTL`, `PERSIST` |
| **Server / Utility** | `PING`, `DBSIZE`, `FLUSHALL` |

---

## 💻 Build and Run

### Run Server:
```bash
g++ -std=c++17 src/data_store.cpp src/resp_parser.cpp src/command_executor.cpp src/tcp_server.cpp src/persistence.cpp src/main.cpp -Iinclude -o redis_server -lws2_32
./redis_server --port 6379
```

### Run Automated Tests:
```bash
g++ -std=c++17 src/data_store.cpp src/resp_parser.cpp src/command_executor.cpp src/persistence.cpp src/test_main.cpp -Iinclude -o test_main
./test_main
```
