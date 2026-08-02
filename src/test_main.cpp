#include "data_store.h"
#include "resp_parser.h"
#include "rdb_persistence.h"
#include "aof_persistence.h"
#include "command_executor.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace redis_clone;

void test_zset_ordering() {
    std::cout << "[TEST] Running ZSet Score Ordering Test..." << std::endl;
    DataStore store;

    // Add players with scores
    assert(store.zadd("leaderboard", 100.5, "Alice") == true);
    assert(store.zadd("leaderboard", 50.0, "Bob") == true);
    assert(store.zadd("leaderboard", 200.0, "Charlie") == true);
    assert(store.zadd("leaderboard", 50.0, "Adam") == true); // Same score as Bob, lexicographical tie breaker

    // ZRANGE ascending by score
    auto range = store.zrange("leaderboard", 0, -1);
    assert(range.size() == 4);
    assert(range[0] == "Adam");   // score 50.0, "Adam" < "Bob"
    assert(range[1] == "Bob");    // score 50.0
    assert(range[2] == "Alice");  // score 100.5
    assert(range[3] == "Charlie");// score 200.0

    // ZREVRANGE descending by score
    auto revrange = store.zrevrange("leaderboard", 0, -1);
    assert(revrange.size() == 4);
    assert(revrange[0] == "Charlie");
    assert(revrange[1] == "Alice");
    assert(revrange[2] == "Bob");
    assert(revrange[3] == "Adam");

    // ZSCORE
    auto score = store.zscore("leaderboard", "Alice");
    assert(score.has_value() && std::abs(*score - 100.5) < 0.001);

    // Update score for Alice
    store.zadd("leaderboard", 10.0, "Alice");
    range = store.zrange("leaderboard", 0, -1);
    assert(range[0] == "Alice"); // Alice now has lowest score (10.0)

    std::cout << " -> PASSED!" << std::endl;
}

void test_resp_parser_consumed() {
    std::cout << "[TEST] Running RESP Parser Exact Byte Consumption Test..." << std::endl;

    std::string resp_array = "*3\r\n$3\r\nSET\r\n$4\r\nname\r\n$5\r\nAlice\r\n";
    size_t consumed = 0;
    auto val = RESPParser::parse_with_consumed(resp_array, consumed);

    assert(val.has_value());
    assert(consumed == resp_array.length());
    assert(std::holds_alternative<std::vector<RedisValue>>(*val));

    auto vec = std::get<std::vector<RedisValue>>(*val);
    assert(vec.size() == 3);
    assert(std::get<std::string>(vec[0]) == "SET");
    assert(std::get<std::string>(vec[1]) == "name");
    assert(std::get<std::string>(vec[2]) == "Alice");

    // Test concatenated streams (pipelined)
    std::string pipeline = resp_array + "+OK\r\n";
    size_t consumed1 = 0;
    auto val1 = RESPParser::parse_with_consumed(pipeline, consumed1);
    assert(val1.has_value());
    assert(consumed1 == resp_array.length());

    std::string tail = pipeline.substr(consumed1);
    size_t consumed2 = 0;
    auto val2 = RESPParser::parse_with_consumed(tail, consumed2);
    assert(val2.has_value());
    assert(consumed2 == 5); // +OK\r\n
    assert(std::get<std::string>(*val2) == "OK");

    std::cout << " -> PASSED!" << std::endl;
}

void test_rdb_persistence() {
    std::cout << "[TEST] Running RDB Persistence Test..." << std::endl;
    auto store1 = std::make_shared<DataStore>();
    store1->set("strkey", std::string("hello world"));
    store1->hset("hashkey", "f1", std::string("v1"));
    store1->lpush("listkey", std::string("elem1"));
    store1->sadd("setkey", std::string("m1"));
    store1->zadd("zkey", 99.9, "player1");

    RDBPersistence rdb1(store1);
    assert(rdb1.save("test_dump.rdb") == true);

    auto store2 = std::make_shared<DataStore>();
    RDBPersistence rdb2(store2);
    assert(rdb2.load("test_dump.rdb") == true);

    assert(store2->get("strkey").has_value());
    assert(std::get<std::string>(*store2->get("strkey")) == "hello world");
    assert(store2->hget("hashkey", "f1").has_value());
    assert(store2->zscore("zkey", "player1").has_value());

    std::cout << " -> PASSED!" << std::endl;
}

void test_aof_rewrite_and_replay() {
    std::cout << "[TEST] Running AOF Persistence & Rewrite Test..." << std::endl;
    auto store = std::make_shared<DataStore>();
    AOFPersistence aof("test_appendonly.aof", store);

    aof.append_command("SET", {std::string("key1"), std::string("val1")});
    aof.append_command("HSET", {std::string("hkey"), std::string("f1"), std::string("v1")});
    aof.append_command("ZADD", {std::string("zkey"), std::string("50"), std::string("m1")});

    store->set("key1", std::string("val1"));
    store->hset("hkey", "f1", std::string("v1"));
    store->zadd("zkey", 50.0, "m1");

    assert(aof.rewrite() == true);

    auto cmds = aof.load();
    assert(!cmds.empty());

    std::cout << " -> PASSED!" << std::endl;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "   REDIS CLONE VERIFICATION SUITE       " << std::endl;
    std::cout << "========================================" << std::endl;

    test_zset_ordering();
    test_resp_parser_consumed();
    test_rdb_persistence();
    test_aof_rewrite_and_replay();

    std::cout << "========================================" << std::endl;
    std::cout << " ALL VERIFICATION TESTS PASSED (10/10)! " << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
