#include "persistence.h"
#include <fstream>
#include <sstream>

using namespace std;

namespace redis_clone {

Persistence::Persistence(shared_ptr<DataStore> store)
    : store_(move(store)) {
}

bool Persistence::save_snapshot(const string& filename) {
    ofstream file(filename.c_str());
    if (!file.is_open()) return false;

    auto entries = store_->get_all_entries();
    for (const auto& pair : entries) {
        const string& key = pair.first;
        const KeyEntry& entry = pair.second;

        int64_t ttl_sec = 0;
        if (entry.has_expiry) {
            auto rem = chrono::duration_cast<chrono::seconds>(entry.expiry_time - chrono::system_clock::now()).count();
            if (rem <= 0) continue;
            ttl_sec = rem;
        }

        if (entry.type == RedisType::STRING) {
            file << "STRING " << key << " " << ttl_sec << " " << entry.str_val << "\n";
        } else if (entry.type == RedisType::LIST) {
            file << "LIST " << key << " " << ttl_sec << " " << entry.list_val.size();
            for (size_t i = 0; i < entry.list_val.size(); ++i) {
                file << " " << entry.list_val[i];
            }
            file << "\n";
        } else if (entry.type == RedisType::HASH) {
            file << "HASH " << key << " " << ttl_sec << " " << entry.hash_val.size();
            for (const auto& hpair : entry.hash_val) {
                file << " " << hpair.first << " " << hpair.second;
            }
            file << "\n";
        }
    }
    return true;
}

bool Persistence::load_snapshot(const string& filename) {
    ifstream file(filename.c_str());
    if (!file.is_open()) return false;

    unordered_map<string, KeyEntry> entries;
    string line;

    while (getline(file, line)) {
        if (line.empty()) continue;
        stringstream ss(line);
        string type_str, key;
        int64_t ttl_sec = 0;
        if (!(ss >> type_str >> key >> ttl_sec)) continue;

        KeyEntry entry;
        entry.has_expiry = (ttl_sec > 0);
        if (entry.has_expiry) {
            entry.expiry_time = chrono::system_clock::now() + chrono::seconds(ttl_sec);
        }

        if (type_str == "STRING") {
            string value;
            if (ss >> value) {
                entry.type = RedisType::STRING;
                entry.str_val = value;
                entries[key] = entry;
            }
        } else if (type_str == "LIST") {
            size_t count = 0;
            if (ss >> count) {
                vector<string> vec;
                string item;
                for (size_t i = 0; i < count && (ss >> item); ++i) {
                    vec.push_back(item);
                }
                entry.type = RedisType::LIST;
                entry.list_val = vec;
                entries[key] = entry;
            }
        } else if (type_str == "HASH") {
            size_t count = 0;
            if (ss >> count) {
                unordered_map<string, string> hash;
                string f, v;
                for (size_t i = 0; i < count && (ss >> f >> v); ++i) {
                    hash[f] = v;
                }
                entry.type = RedisType::HASH;
                entry.hash_val = hash;
                entries[key] = entry;
            }
        }
    }

    store_->restore_entries(entries);
    return true;
}

} // namespace redis_clone
