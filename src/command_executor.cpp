#include "command_executor.h"
#include "resp_parser.h"
#include <algorithm>

using namespace std;

namespace redis_clone {

CommandExecutor::CommandExecutor(shared_ptr<DataStore> store)
    : store_(move(store)) {
    register_handlers();
}

void CommandExecutor::register_handlers() {
    // PING
    handlers_["PING"] = [](const vector<string>& args) {
        return args.size() > 1 ? RESPParser::serialize_bulk_string(args[1]) : RESPParser::serialize_simple_string("PONG");
    };

    // SET key val [EX seconds]
    handlers_["SET"] = [this](const vector<string>& args) {
        if (args.size() < 3) return RESPParser::serialize_error("wrong number of arguments for 'set'");
        chrono::milliseconds ttl(0);
        for (size_t i = 3; i < args.size(); ++i) {
            string opt = args[i];
            transform(opt.begin(), opt.end(), opt.begin(), ::toupper);
            if (opt == "EX" && i + 1 < args.size()) {
                try { ttl = chrono::seconds(stoll(args[++i])); } catch (...) {}
            }
        }
        store_->set(args[1], args[2], ttl);
        return RESPParser::serialize_simple_string("OK");
    };

    // GET key
    handlers_["GET"] = [this](const vector<string>& args) {
        if (args.size() != 2) return RESPParser::serialize_error("wrong number of arguments for 'get'");
        string val;
        if (store_->get(args[1], val)) {
            return RESPParser::serialize_bulk_string(val);
        }
        return RESPParser::serialize_null();
    };

    // DEL key [key ...]
    handlers_["DEL"] = [this](const vector<string>& args) {
        if (args.size() < 2) return RESPParser::serialize_error("wrong number of arguments for 'del'");
        int64_t count = 0;
        for (size_t i = 1; i < args.size(); ++i) {
            if (store_->del(args[i])) count++;
        }
        return RESPParser::serialize_integer(count);
    };

    // EXISTS key [key ...]
    handlers_["EXISTS"] = [this](const vector<string>& args) {
        if (args.size() < 2) return RESPParser::serialize_error("wrong number of arguments for 'exists'");
        int64_t count = 0;
        for (size_t i = 1; i < args.size(); ++i) {
            if (store_->exists(args[i])) count++;
        }
        return RESPParser::serialize_integer(count);
    };

    // KEYS [pattern]
    handlers_["KEYS"] = [this](const vector<string>& args) {
        string pattern = args.size() > 1 ? args[1] : "*";
        return RESPParser::serialize_array(store_->keys(pattern));
    };

    // EXPIRE key seconds
    handlers_["EXPIRE"] = [this](const vector<string>& args) {
        if (args.size() != 3) return RESPParser::serialize_error("wrong number of arguments for 'expire'");
        try {
            int64_t sec = stoll(args[2]);
            return RESPParser::serialize_integer(store_->expire(args[1], chrono::seconds(sec)) ? 1 : 0);
        } catch (...) {
            return RESPParser::serialize_error("value is not an integer or out of range");
        }
    };

    // TTL key
    handlers_["TTL"] = [this](const vector<string>& args) {
        if (args.size() != 2) return RESPParser::serialize_error("wrong number of arguments for 'ttl'");
        return RESPParser::serialize_integer(store_->ttl(args[1]));
    };

    // PERSIST key
    handlers_["PERSIST"] = [this](const vector<string>& args) {
        if (args.size() != 2) return RESPParser::serialize_error("wrong number of arguments for 'persist'");
        return RESPParser::serialize_integer(store_->persist(args[1]) ? 1 : 0);
    };

    // LPUSH key val [val ...]
    handlers_["LPUSH"] = [this](const vector<string>& args) {
        if (args.size() < 3) return RESPParser::serialize_error("wrong number of arguments for 'lpush'");
        size_t len = 0;
        for (size_t i = 2; i < args.size(); ++i) len = store_->lpush(args[1], args[i]);
        return RESPParser::serialize_integer(static_cast<int64_t>(len));
    };

    // RPUSH key val [val ...]
    handlers_["RPUSH"] = [this](const vector<string>& args) {
        if (args.size() < 3) return RESPParser::serialize_error("wrong number of arguments for 'rpush'");
        size_t len = 0;
        for (size_t i = 2; i < args.size(); ++i) len = store_->rpush(args[1], args[i]);
        return RESPParser::serialize_integer(static_cast<int64_t>(len));
    };

    // LPOP key
    handlers_["LPOP"] = [this](const vector<string>& args) {
        if (args.size() != 2) return RESPParser::serialize_error("wrong number of arguments for 'lpop'");
        string val;
        if (store_->lpop(args[1], val)) {
            return RESPParser::serialize_bulk_string(val);
        }
        return RESPParser::serialize_null();
    };

    // RPOP key
    handlers_["RPOP"] = [this](const vector<string>& args) {
        if (args.size() != 2) return RESPParser::serialize_error("wrong number of arguments for 'rpop'");
        string val;
        if (store_->rpop(args[1], val)) {
            return RESPParser::serialize_bulk_string(val);
        }
        return RESPParser::serialize_null();
    };

    // LRANGE key start stop
    handlers_["LRANGE"] = [this](const vector<string>& args) {
        if (args.size() != 4) return RESPParser::serialize_error("wrong number of arguments for 'lrange'");
        try {
            int64_t start = stoll(args[2]);
            int64_t stop = stoll(args[3]);
            return RESPParser::serialize_array(store_->lrange(args[1], start, stop));
        } catch (...) {
            return RESPParser::serialize_error("value is not an integer or out of range");
        }
    };

    // LLEN key
    handlers_["LLEN"] = [this](const vector<string>& args) {
        if (args.size() != 2) return RESPParser::serialize_error("wrong number of arguments for 'llen'");
        return RESPParser::serialize_integer(static_cast<int64_t>(store_->llen(args[1])));
    };

    // HSET key field val
    handlers_["HSET"] = [this](const vector<string>& args) {
        if (args.size() < 4) return RESPParser::serialize_error("wrong number of arguments for 'hset'");
        bool res = store_->hset(args[1], args[2], args[3]);
        return RESPParser::serialize_integer(res ? 1 : 0);
    };

    // HGET key field
    handlers_["HGET"] = [this](const vector<string>& args) {
        if (args.size() != 3) return RESPParser::serialize_error("wrong number of arguments for 'hget'");
        string val;
        if (store_->hget(args[1], args[2], val)) {
            return RESPParser::serialize_bulk_string(val);
        }
        return RESPParser::serialize_null();
    };

    // HDEL key field [field ...]
    handlers_["HDEL"] = [this](const vector<string>& args) {
        if (args.size() < 3) return RESPParser::serialize_error("wrong number of arguments for 'hdel'");
        int64_t count = 0;
        for (size_t i = 2; i < args.size(); ++i) {
            if (store_->hdel(args[1], args[i])) count++;
        }
        return RESPParser::serialize_integer(count);
    };

    // HGETALL key
    handlers_["HGETALL"] = [this](const vector<string>& args) {
        if (args.size() != 2) return RESPParser::serialize_error("wrong number of arguments for 'hgetall'");
        auto pairs = store_->hgetall(args[1]);
        vector<string> flat;
        flat.reserve(pairs.size() * 2);
        for (const auto& pair : pairs) {
            flat.push_back(pair.first);
            flat.push_back(pair.second);
        }
        return RESPParser::serialize_array(flat);
    };

    // HLEN key
    handlers_["HLEN"] = [this](const vector<string>& args) {
        if (args.size() != 2) return RESPParser::serialize_error("wrong number of arguments for 'hlen'");
        return RESPParser::serialize_integer(static_cast<int64_t>(store_->hlen(args[1])));
    };

    // DBSIZE
    handlers_["DBSIZE"] = [this](const vector<string>&) {
        return RESPParser::serialize_integer(static_cast<int64_t>(store_->dbsize()));
    };

    // FLUSHALL / FLUSHDB
    handlers_["FLUSHALL"] = [this](const vector<string>&) {
        store_->flushall();
        return RESPParser::serialize_simple_string("OK");
    };
    handlers_["FLUSHDB"] = handlers_["FLUSHALL"];
}

string CommandExecutor::execute(const vector<string>& args) {
    if (args.empty()) return RESPParser::serialize_error("empty command");

    string cmd = args[0];
    transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    auto it = handlers_.find(cmd);
    if (it != handlers_.end()) {
        return it->second(args);
    }
    return RESPParser::serialize_error("unknown command '" + args[0] + "'");
}

} // namespace redis_clone
