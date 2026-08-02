#ifndef RESP_PARSER_H
#define RESP_PARSER_H

#include "redis_clone.h"
#include <string>
#include <vector>
#include <optional>

namespace redis_clone {

class RESPParser {
public:
    // Parse RESP protocol data into RedisValue
    static std::optional<RedisValue> parse(const std::string& data);
    
    // Parse RESP protocol data into RedisValue and report exact consumed bytes
    static std::optional<RedisValue> parse_with_consumed(const std::string& data, size_t& consumed_bytes);
    
    // Serialize RedisValue to RESP protocol
    static std::string serialize(const RedisValue& value);
    
    // Parse array
    static std::optional<std::vector<RedisValue>> parse_array(const std::string& data);
    
    // Serialize array
    static std::string serialize_array(const std::vector<RedisValue>& values);
    
    // Parse bulk string
    static std::optional<std::string> parse_bulk_string(const std::string& data);
    
    // Serialize bulk string
    static std::string serialize_bulk_string(const std::string& str);
    
    // Parse simple string
    static std::optional<std::string> parse_simple_string(const std::string& data);
    
    // Serialize simple string
    static std::string serialize_simple_string(const std::string& str);
    
    // Parse integer
    static std::optional<int64_t> parse_integer(const std::string& data);
    
    // Serialize integer
    static std::string serialize_integer(int64_t value);
    
    // Parse error
    static std::optional<std::string> parse_error(const std::string& data);
    
    // Serialize error
    static std::string serialize_error(const std::string& error);
    
    // Serialize null
    static std::string serialize_null();
    
    // Serialize OK response
    static std::string serialize_ok();
    
    // Serialize PONG response
    static std::string serialize_pong();

private:
    static size_t find_crlf(const std::string& data, size_t start);
    static std::string read_line(const std::string& data, size_t& pos);
};

} // namespace redis_clone

#endif // RESP_PARSER_H
