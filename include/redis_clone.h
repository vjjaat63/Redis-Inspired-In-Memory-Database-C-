#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <cstdint>

using namespace std;

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    namespace redis_clone {
        class Mutex {
            CRITICAL_SECTION cs_;
        public:
            Mutex() { InitializeCriticalSection(&cs_); }
            ~Mutex() { DeleteCriticalSection(&cs_); }
            void lock() { EnterCriticalSection(&cs_); }
            void unlock() { LeaveCriticalSection(&cs_); }
        };
        class LockGuard {
            Mutex& m_;
        public:
            explicit LockGuard(Mutex& m) : m_(m) { m_.lock(); }
            ~LockGuard() { m_.unlock(); }
        };
    }
#else
    #include <mutex>
    namespace redis_clone {
        using Mutex = mutex;
        using LockGuard = lock_guard<mutex>;
    }
#endif

namespace redis_clone {

// Supported Redis data types
enum class RedisType {
    STRING,
    LIST,
    HASH,
    NONE
};

// In-Memory Key Entry containing standard STL containers and TTL metadata
struct KeyEntry {
    RedisType type{RedisType::NONE};
    string str_val;
    vector<string> list_val;
    unordered_map<string, string> hash_val;
    chrono::system_clock::time_point expiry_time;
    bool has_expiry{false};
};

} // namespace redis_clone
