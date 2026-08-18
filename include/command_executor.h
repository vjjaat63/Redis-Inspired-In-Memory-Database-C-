#pragma once

#include "data_store.h"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

using namespace std;

namespace redis_clone {

class CommandExecutor {
public:
    explicit CommandExecutor(shared_ptr<DataStore> store);

    // Executes a parsed command using STL dispatch table
    string execute(const vector<string>& args);

private:
    shared_ptr<DataStore> store_;
    unordered_map<string, function<string(const vector<string>&)>> handlers_;

    void register_handlers();
};

} // namespace redis_clone
