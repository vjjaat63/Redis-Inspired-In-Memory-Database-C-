#pragma once

#include "data_store.h"
#include <memory>
#include <string>

using namespace std;

namespace redis_clone {

class Persistence {
public:
    explicit Persistence(shared_ptr<DataStore> store);

    // Save full database snapshot to file
    bool save_snapshot(const string& filename);

    // Load and restore database snapshot from file
    bool load_snapshot(const string& filename);

private:
    shared_ptr<DataStore> store_;
};

} // namespace redis_clone
