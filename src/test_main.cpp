#include "data_store.h"
#include "resp_parser.h"
#include "command_executor.h"
#include "persistence.h"
#include <iostream>
#include <cassert>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
    void portable_sleep(int ms) { Sleep(ms); }
#else
    #include <unistd.h>
    void portable_sleep(int ms) { usleep(ms * 1000); }
#endif

using namespace std;
using namespace redis_clone;

void test_strings() {
    cout << "[TEST] 1. String Operations...";
    DataStore store;
    assert(store.set("name", "Alice"));
    string val;
    assert(store.get("name", val) && val == "Alice");
    assert(store.exists("name") == true);
    assert(store.del("name") == true);
    assert(store.exists("name") == false);
    assert(store.get("name", val) == false);
    cout << " PASSED!" << endl;
}

void test_lists() {
    cout << "[TEST] 2. List Operations...";
    DataStore store;
    assert(store.rpush("mylist", "a") == 1);
    assert(store.rpush("mylist", "b") == 2);
    assert(store.lpush("mylist", "start") == 3);
    assert(store.llen("mylist") == 3);

    auto range = store.lrange("mylist", 0, -1);
    assert(range.size() == 3);
    assert(range[0] == "start" && range[1] == "a" && range[2] == "b");

    string val;
    assert(store.lpop("mylist", val) && val == "start");
    assert(store.rpop("mylist", val) && val == "b");
    assert(store.llen("mylist") == 1);
    cout << " PASSED!" << endl;
}

void test_hashes() {
    cout << "[TEST] 3. Hash Operations...";
    DataStore store;
    assert(store.hset("user:1", "name", "Bob"));
    assert(store.hset("user:1", "age", "30"));
    assert(store.hlen("user:1") == 2);
    string val;
    assert(store.hget("user:1", "name", val) && val == "Bob");
    assert(store.hget("user:1", "age", val) && val == "30");

    auto all = store.hgetall("user:1");
    assert(all.size() == 2);

    assert(store.hdel("user:1", "age") == true);
    assert(store.hlen("user:1") == 1);
    assert(store.hget("user:1", "age", val) == false);
    cout << " PASSED!" << endl;
}

void test_expiration() {
    cout << "[TEST] 4. Key Expiration & TTL...";
    DataStore store;
    store.set("session", "xyz", chrono::milliseconds(50));
    string val;
    assert(store.get("session", val) && val == "xyz");
    assert(store.ttl("session") >= 0);

    portable_sleep(60);
    assert(store.get("session", val) == false);
    assert(store.ttl("session") == -2);
    cout << " PASSED!" << endl;
}

void test_resp_parser_and_pipelining() {
    cout << "[TEST] 5. RESP Protocol & Pipelining...";
    string stream = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";

    size_t consumed1 = 0;
    vector<string> cmd1;
    assert(RESPParser::parse_command(stream, cmd1, consumed1) && cmd1.size() == 3);
    assert(cmd1[0] == "SET" && cmd1[1] == "foo" && cmd1[2] == "bar");

    string tail = stream.substr(consumed1);
    size_t consumed2 = 0;
    vector<string> cmd2;
    assert(RESPParser::parse_command(tail, cmd2, consumed2) && cmd2.size() == 2);
    assert(cmd2[0] == "GET" && cmd2[1] == "foo");
    cout << " PASSED!" << endl;
}

#ifdef _WIN32
struct ThreadParam {
    DataStore* store;
    int thread_id;
    int ops;
};

DWORD WINAPI WorkerFunc(LPVOID lpParam) {
    ThreadParam* p = static_cast<ThreadParam*>(lpParam);
    for (int i = 0; i < p->ops; ++i) {
        string key = "key_" + to_string(p->thread_id) + "_" + to_string(i);
        p->store->set(key, "val");
        string val;
        assert(p->store->get(key, val) && val == "val");
    }
    return 0;
}
#endif

void test_multithreading_concurrency() {
    cout << "[TEST] 6. Multithreading & Mutex Synchronization (1,000 Concurrent Writes)...";
    auto store = make_shared<DataStore>();
    const int NUM_THREADS = 10;
    const int OPS_PER_THREAD = 100;

#ifdef _WIN32
    HANDLE handles[NUM_THREADS];
    ThreadParam params[NUM_THREADS];
    for (int t = 0; t < NUM_THREADS; ++t) {
        params[t].store = store.get();
        params[t].thread_id = t;
        params[t].ops = OPS_PER_THREAD;
        handles[t] = CreateThread(NULL, 0, WorkerFunc, &params[t], 0, NULL);
    }
    WaitForMultipleObjects(NUM_THREADS, handles, TRUE, INFINITE);
    for (int t = 0; t < NUM_THREADS; ++t) {
        CloseHandle(handles[t]);
    }
#endif

    assert(store->dbsize() == NUM_THREADS * OPS_PER_THREAD);
    cout << " PASSED!" << endl;
}

void test_snapshot_persistence() {
    cout << "[TEST] 7. File Snapshot Persistence & Crash Recovery...";
    auto store1 = make_shared<DataStore>();
    store1->set("site", "redis.io");
    store1->hset("config", "maxclients", "10000");
    store1->rpush("queue", "task1");
    store1->rpush("queue", "task2");

    Persistence p1(store1);
    assert(p1.save_snapshot("test_snapshot.rdb") == true);

    auto store2 = make_shared<DataStore>();
    Persistence p2(store2);
    assert(p2.load_snapshot("test_snapshot.rdb") == true);

    string val;
    assert(store2->get("site", val) && val == "redis.io");
    assert(store2->hget("config", "maxclients", val) && val == "10000");
    assert(store2->llen("queue") == 2);
    assert(store2->lpop("queue", val) && val == "task1");
    cout << " PASSED!" << endl;
}

int main() {
    cout << "===============================================" << endl;
    cout << "  MINIMIZED REDIS CLONE VERIFICATION SUITE     " << endl;
    cout << "===============================================" << endl;

    test_strings();
    test_lists();
    test_hashes();
    test_expiration();
    test_resp_parser_and_pipelining();
    test_multithreading_concurrency();
    test_snapshot_persistence();

    cout << "===============================================" << endl;
    cout << "  ALL 7 CORE VERIFICATION SUITES PASSED!       " << endl;
    cout << "===============================================" << endl;
    return 0;
}
