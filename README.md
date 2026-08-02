# Redis Clone - C++ Implementation

A placement-level, full-fledged Redis clone implemented in C++17. This project demonstrates a complete in-memory key-value store with Redis protocol compatibility, persistence, and comprehensive data structure support.

## Features

### Core Data Structures
- **Strings** - Basic key-value storage
- **Hashes** - Field-value pairs
- **Lists** - Ordered collections
- **Sets** - Unordered unique collections
- **Sorted Sets** - Ordered sets with scores

### Key Features
- **RESP Protocol** - Full Redis Serialization Protocol implementation
- **TCP Server** - Multi-client connection handling
- **Thread Safety** - Shared mutex for concurrent access
- **Key Expiration** - TTL support with automatic cleanup
- **Persistence** - RDB (snapshot) and AOF (append-only file) support
- **Configuration** - Redis-style configuration file support
- **Logging** - Comprehensive logging system with multiple levels

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     Redis Server                         │
├─────────────────────────────────────────────────────────┤
│  TCP Server  │  Command Executor  │  Persistence Layer   │
├─────────────────────────────────────────────────────────┤
│              RESP Parser / Serializer                    │
├─────────────────────────────────────────────────────────┤
│                   Data Store                             │
│  ┌─────────┬─────────┬─────────┬─────────┬──────────┐  │
│  │ String  │  Hash   │  List   │  Set    │ Sorted   │  │
│  │         │         │         │         │   Set    │  │
│  └─────────┴─────────┴─────────┴─────────┴──────────┘  │
├─────────────────────────────────────────────────────────┤
│              Thread Safety (Shared Mutex)                │
└─────────────────────────────────────────────────────────┘
```

## Project Structure

```
redis-clone/
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── include/                    # Header files
│   ├── redis_clone.h          # Main definitions
│   ├── data_store.h           # Key-value store
│   ├── resp_parser.h          # RESP protocol
│   ├── tcp_server.h           # Network server
│   ├── command_executor.h     # Command handlers
│   ├── rdb_persistence.h      # RDB snapshot
│   ├── aof_persistence.h      # AOF logging
│   ├── config.h               # Configuration
│   ├── logger.h               # Logging system
│   └── redis_server.h         # Main server class
└── src/                       # Implementation files
    ├── data_store.cpp
    ├── resp_parser.cpp
    ├── tcp_server.cpp
    ├── command_executor.cpp
    ├── rdb_persistence.cpp
    ├── aof_persistence.cpp
    ├── config.cpp
    ├── logger.cpp
    ├── redis_server.cpp
    └── main.cpp
```

## Building

### Prerequisites
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.14 or higher
- (Linux) pthread library

### Build Instructions

#### Linux/macOS
```bash
mkdir build
cd build
cmake ..
make
```

#### Windows (Visual Studio)
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

#### Windows (MinGW)
```bash
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
mingw32-make
```

The executable will be named `redis_server` (or `redis_server.exe` on Windows).

## Usage

### Basic Usage

Start the server with default settings:
```bash
./redis_server
```

Start on a custom port:
```bash
./redis_server --port 6380
```

Load configuration file:
```bash
./redis_server --config redis.conf
```

Set log level and file:
```bash
./redis_server --log-level debug --log-file redis.log
```

### Command Line Options
- `--port <port>` - Set server port (default: 6379)
- `--config <file>` - Load configuration from file
- `--log-level <level>` - Set log level (debug, info, warning, error, fatal)
- `--log-file <file>` - Set log file path
- `--help` - Show help message

### Connecting with redis-cli

```bash
redis-cli -p 6379
```

## Supported Commands

### String Commands
- `SET key value [EX seconds]` - Set key-value pair with optional expiration
- `GET key` - Get value by key
- `DEL key [key ...]` - Delete one or more keys
- `EXISTS key [key ...]` - Check if keys exist
- `TYPE key` - Get the type of key
- `EXPIRE key seconds` - Set expiration time
- `TTL key` - Get time to live
- `PERSIST key` - Remove expiration

### Hash Commands
- `HSET key field value` - Set hash field
- `HGET key field` - Get hash field
- `HDEL key field [field ...]` - Delete hash fields
- `HKEYS key` - Get all hash fields
- `HVALS key` - Get all hash values
- `HGETALL key` - Get all fields and values
- `HEXISTS key field` - Check if field exists
- `HLEN key` - Get number of fields

### List Commands
- `LPUSH key value [value ...]` - Push to left
- `RPUSH key value [value ...]` - Push to right
- `LPOP key` - Pop from left
- `RPOP key` - Pop from right
- `LRANGE key start stop` - Get range of elements
- `LLEN key` - Get list length
- `LINDEX key index` - Get element by index

### Set Commands
- `SADD key member [member ...]` - Add members
- `SREM key member [member ...]` - Remove members
- `SISMEMBER key member` - Check if member exists
- `SMEMBERS key` - Get all members
- `SCARD key` - Get set size

### Sorted Set Commands
- `ZADD key score member [score member ...]` - Add members with scores
- `ZREM key member [member ...]` - Remove members
- `ZSCORE key member` - Get score of member
- `ZRANGE key start stop` - Get range by score (ascending)
- `ZREVRANGE key start stop` - Get range by score (descending)
- `ZCARD key` - Get sorted set size

### Server Commands
- `PING [message]` - Ping server
- `DBSIZE` - Get number of keys in database
- `KEYS pattern` - Find keys matching pattern
- `FLUSHDB` - Clear current database
- `FLUSHALL` - Clear all databases

## Configuration

Create a `redis.conf` file with the following options:

```
# Network
port 6379
bind 127.0.0.1

# General
daemonize no
pidfile /var/run/redis.pid

# Logging
loglevel notice
logfile ""

# Database
databases 16

# Snapshotting
save 900 1
save 300 10
save 60 10000

# AOF
appendonly no
appendfilename appendonly.aof
appendfsync everysec

# Limits
maxclients 10000
timeout 0
tcp-keepalive 300
```

## Persistence

### RDB (Snapshot)
RDB persistence creates point-in-time snapshots of the dataset.

```cpp
// Save snapshot
server->save_rdb("dump.rdb");

// Load snapshot
server->load_rdb("dump.rdb");
```

### AOF (Append Only File)
AOF logs every write operation received by the server.

```cpp
// Enable AOF
server->enable_aof(true, "appendonly.aof");
```

## Examples

### String Operations
```
SET mykey "Hello World"
GET mykey
EXISTS mykey
EXPIRE mykey 60
TTL mykey
```

### Hash Operations
```
HSET user:1 name "John Doe"
HSET user:1 email "john@example.com"
HGET user:1 name
HGETALL user:1
```

### List Operations
```
LPUSH mylist "item1"
LPUSH mylist "item2"
LRANGE mylist 0 -1
LPOP mylist
```

### Set Operations
```
SADD myset "member1"
SADD myset "member2"
SMEMBERS myset
SISMEMBER myset "member1"
```

### Sorted Set Operations
```
ZADD leaderboard 100 "player1"
ZADD leaderboard 200 "player2"
ZRANGE leaderboard 0 -1
ZSCORE leaderboard "player1"
```

## Testing with redis-cli

```bash
# Start the server
./redis_server --port 6379

# In another terminal, connect with redis-cli
redis-cli -p 6379

# Test commands
127.0.0.1:6379> PING
PONG

127.0.0.1:6379> SET mykey "Hello"
OK

127.0.0.1:6379> GET mykey
"Hello"

127.0.0.1:6379> DBSIZE
(integer) 1
```

## Implementation Details

### Thread Safety
- Uses `std::shared_mutex` for read-write locking
- Multiple readers can access data simultaneously
- Writers have exclusive access

### RESP Protocol
- Implements Redis Serialization Protocol (RESP)
- Supports simple strings, errors, integers, bulk strings, and arrays
- Full serialization and deserialization

### Key Expiration
- Keys can have TTL set via `EXPIRE` command or `SET ... EX` option
- Background thread cleans up expired keys every second
- Thread-safe expiration checking

### Memory Management
- Uses smart pointers (`std::shared_ptr`, `std::unique_ptr`)
- RAII pattern for resource management
- No memory leaks

## Limitations

This is a placement-level implementation for educational purposes. Some limitations:
- Simplified RDB format (not fully compatible with Redis)
- Basic AOF implementation
- No clustering or replication
- No Lua scripting
- No pub/sub
- No transactions (MULTI/EXEC)
- No modules
- Limited to single database

## Future Enhancements

- Full RDB compatibility
- Transaction support (MULTI/EXEC)
- Pub/Sub messaging
- Lua scripting
- Cluster support
- Replication
- More data types (HyperLogLog, Bitmaps, Geospatial)
- Performance optimizations

## License

This project is for educational purposes.

## Contributing

This is a learning project. Feel free to fork and modify for your own educational purposes.

## Author

Created as a placement-level demonstration of C++ systems programming and Redis architecture understanding.
