#pragma once

#include "redis_clone.h"
#include <string>
#include <vector>

using namespace std;

namespace redis_clone {

class RESPParser {
public:
    // Parse RESP protocol string, populating parsed array of tokens and consumed byte count
    static bool parse_command(const string& data, vector<string>& out_args, size_t& consumed);

    // Serialization methods for standard RESP data types
    static string serialize_simple_string(const string& str);
    static string serialize_error(const string& err);
    static string serialize_integer(int64_t val);
    static string serialize_bulk_string(const string& str);
    static string serialize_null();
    static string serialize_array(const vector<string>& elements);
};

} // namespace redis_clone
