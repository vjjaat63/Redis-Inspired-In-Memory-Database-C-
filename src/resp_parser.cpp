#include "resp_parser.h"
#include <sstream>
#include <stdexcept>

namespace redis_clone {

std::optional<RedisValue> RESPParser::parse(const std::string& data) {
    size_t dummy = 0;
    return parse_with_consumed(data, dummy);
}

std::optional<RedisValue> RESPParser::parse_with_consumed(const std::string& data, size_t& consumed_bytes) {
    consumed_bytes = 0;
    if (data.empty()) {
        return std::nullopt;
    }
    
    char type = data[0];
    
    switch (type) {
        case '+': { // Simple String: +OK\r\n
            size_t crlf = find_crlf(data, 1);
            if (crlf == std::string::npos) return std::nullopt;
            consumed_bytes = crlf + 2;
            return data.substr(1, crlf - 1);
        }
        case '-': { // Error: -ERR message\r\n
            size_t crlf = find_crlf(data, 1);
            if (crlf == std::string::npos) return std::nullopt;
            consumed_bytes = crlf + 2;
            return data.substr(1, crlf - 1);
        }
        case ':': { // Integer: :1000\r\n
            size_t crlf = find_crlf(data, 1);
            if (crlf == std::string::npos) return std::nullopt;
            try {
                int64_t val = std::stoll(data.substr(1, crlf - 1));
                consumed_bytes = crlf + 2;
                return val;
            } catch (...) {
                return std::nullopt;
            }
        }
        case '$': { // Bulk String: $6\r\nfoobar\r\n or $-1\r\n
            size_t crlf = find_crlf(data, 1);
            if (crlf == std::string::npos) return std::nullopt;
            try {
                int length = std::stoi(data.substr(1, crlf - 1));
                if (length == -1) {
                    consumed_bytes = crlf + 2;
                    return std::nullptr_t{};
                }
                if (length < 0) return std::nullopt;
                size_t total_needed = crlf + 2 + static_cast<size_t>(length) + 2;
                if (data.length() < total_needed) return std::nullopt;
                consumed_bytes = total_needed;
                return data.substr(crlf + 2, length);
            } catch (...) {
                return std::nullopt;
            }
        }
        case '*': { // Array: *2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n
            size_t crlf = find_crlf(data, 1);
            if (crlf == std::string::npos) return std::nullopt;
            try {
                int count = std::stoi(data.substr(1, crlf - 1));
                if (count == -1) {
                    consumed_bytes = crlf + 2;
                    return std::vector<RedisValue>{};
                }
                if (count < 0) return std::nullopt;

                size_t curr_pos = crlf + 2;
                std::vector<RedisValue> array_result;
                array_result.reserve(count);

                for (int i = 0; i < count; ++i) {
                    if (curr_pos >= data.length()) return std::nullopt;
                    size_t elem_consumed = 0;
                    auto elem = parse_with_consumed(data.substr(curr_pos), elem_consumed);
                    if (!elem || elem_consumed == 0) return std::nullopt;
                    array_result.push_back(*elem);
                    curr_pos += elem_consumed;
                }
                consumed_bytes = curr_pos;
                return array_result;
            } catch (...) {
                return std::nullopt;
            }
        }
        default:
            return std::nullopt;
    }
}

std::string RESPParser::serialize(const RedisValue& value) {
    if (std::holds_alternative<std::nullptr_t>(value)) {
        return serialize_null();
    } else if (std::holds_alternative<std::string>(value)) {
        return serialize_bulk_string(std::get<std::string>(value));
    } else if (std::holds_alternative<int64_t>(value)) {
        return serialize_integer(std::get<int64_t>(value));
    } else if (std::holds_alternative<std::vector<RedisValue>>(value)) {
        return serialize_array(std::get<std::vector<RedisValue>>(value));
    } else if (std::holds_alternative<std::unordered_map<std::string, RedisValue>>(value)) {
        // Serialize hash as array of field-value pairs
        const auto& hash = std::get<std::unordered_map<std::string, RedisValue>>(value);
        std::vector<RedisValue> array;
        for (const auto& [field, val] : hash) {
            array.push_back(field);
            array.push_back(val);
        }
        return serialize_array(array);
    } else if (std::holds_alternative<std::set<std::string>>(value)) {
        // Serialize set as array
        const auto& set = std::get<std::set<std::string>>(value);
        std::vector<RedisValue> array;
        for (const auto& member : set) {
            array.push_back(member);
        }
        return serialize_array(array);
    } else if (std::holds_alternative<std::list<std::string>>(value)) {
        // Serialize list as array
        const auto& list = std::get<std::list<std::string>>(value);
        std::vector<RedisValue> array;
        for (const auto& item : list) {
            array.push_back(item);
        }
        return serialize_array(array);
    } else if (std::holds_alternative<ZSet>(value)) {
        // Serialize sorted set as array of member-score pairs
        const auto& zset = std::get<ZSet>(value);
        std::vector<RedisValue> array;
        for (const auto& [score, member] : zset.score_set) {
            array.push_back(member);
            array.push_back(std::to_string(score));
        }
        return serialize_array(array);
    }
    
    return serialize_null();
}

std::optional<std::vector<RedisValue>> RESPParser::parse_array(const std::string& data) {
    size_t consumed = 0;
    auto val = parse_with_consumed(data, consumed);
    if (val && std::holds_alternative<std::vector<RedisValue>>(*val)) {
        return std::get<std::vector<RedisValue>>(*val);
    }
    return std::nullopt;
}

std::string RESPParser::serialize_array(const std::vector<RedisValue>& values) {
    std::ostringstream oss;
    oss << "*" << values.size() << "\r\n";
    for (const auto& value : values) {
        oss << serialize(value);
    }
    return oss.str();
}

std::optional<std::string> RESPParser::parse_bulk_string(const std::string& data) {
    size_t consumed = 0;
    auto val = parse_with_consumed(data, consumed);
    if (val && std::holds_alternative<std::string>(*val)) {
        return std::get<std::string>(*val);
    }
    return std::nullopt;
}

std::string RESPParser::serialize_bulk_string(const std::string& str) {
    std::ostringstream oss;
    oss << "$" << str.length() << "\r\n" << str << "\r\n";
    return oss.str();
}

std::optional<std::string> RESPParser::parse_simple_string(const std::string& data) {
    size_t consumed = 0;
    auto val = parse_with_consumed(data, consumed);
    if (val && std::holds_alternative<std::string>(*val)) {
        return std::get<std::string>(*val);
    }
    return std::nullopt;
}

std::string RESPParser::serialize_simple_string(const std::string& str) {
    return "+" + str + "\r\n";
}

std::optional<int64_t> RESPParser::parse_integer(const std::string& data) {
    size_t consumed = 0;
    auto val = parse_with_consumed(data, consumed);
    if (val && std::holds_alternative<int64_t>(*val)) {
        return std::get<int64_t>(*val);
    }
    return std::nullopt;
}

std::string RESPParser::serialize_integer(int64_t value) {
    return ":" + std::to_string(value) + "\r\n";
}

std::optional<std::string> RESPParser::parse_error(const std::string& data) {
    size_t consumed = 0;
    auto val = parse_with_consumed(data, consumed);
    if (val && std::holds_alternative<std::string>(*val)) {
        return std::get<std::string>(*val);
    }
    return std::nullopt;
}

std::string RESPParser::serialize_error(const std::string& error) {
    return "-" + error + "\r\n";
}

std::string RESPParser::serialize_null() {
    return "$-1\r\n";
}

std::string RESPParser::serialize_ok() {
    return "+OK\r\n";
}

std::string RESPParser::serialize_pong() {
    return "+PONG\r\n";
}

size_t RESPParser::find_crlf(const std::string& data, size_t start) {
    if (start >= data.length()) return std::string::npos;
    size_t pos = data.find("\r\n", start);
    return pos;
}

std::string RESPParser::read_line(const std::string& data, size_t& pos) {
    size_t crlf_pos = find_crlf(data, pos);
    if (crlf_pos == std::string::npos) {
        pos = data.length();
        return "";
    }
    std::string line = data.substr(pos, crlf_pos - pos);
    pos = crlf_pos + 2;
    return line;
}

} // namespace redis_clone
