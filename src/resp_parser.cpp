#include "resp_parser.h"
#include <sstream>

using namespace std;

namespace redis_clone {

bool RESPParser::parse_command(const string& data, vector<string>& out_args, size_t& consumed) {
    out_args.clear();
    consumed = 0;
    if (data.empty()) return false;

    // Handle standard RESP Array format (e.g. *3\r\n$3\r\nSET\r\n...)
    if (data[0] == '*') {
        size_t crlf = data.find("\r\n", 1);
        if (crlf == string::npos) return false;

        int count = 0;
        try { count = stoi(data.substr(1, crlf - 1)); } catch (...) { return false; }
        if (count < 0) return false;

        size_t pos = crlf + 2;
        out_args.reserve(count);

        for (int i = 0; i < count; ++i) {
            if (pos >= data.length() || data[pos] != '$') return false;
            size_t len_crlf = data.find("\r\n", pos + 1);
            if (len_crlf == string::npos) return false;

            int len = 0;
            try { len = stoi(data.substr(pos + 1, len_crlf - pos - 1)); } catch (...) { return false; }
            if (len < 0) return false;

            size_t str_start = len_crlf + 2;
            if (str_start + len + 2 > data.length()) return false;
            if (data.substr(str_start + len, 2) != "\r\n") return false;

            out_args.push_back(data.substr(str_start, len));
            pos = str_start + len + 2;
        }

        consumed = pos;
        return true;
    }

    // Handle Inline commands (e.g. "PING\r\n" or "SET a b\r\n")
    size_t line_end = data.find("\r\n");
    if (line_end == string::npos) return false;

    string line = data.substr(0, line_end);
    stringstream ss(line);
    string token;
    while (ss >> token) {
        out_args.push_back(token);
    }

    consumed = line_end + 2;
    return !out_args.empty();
}

string RESPParser::serialize_simple_string(const string& str) {
    return "+" + str + "\r\n";
}

string RESPParser::serialize_error(const string& err) {
    return "-ERR " + err + "\r\n";
}

string RESPParser::serialize_integer(int64_t val) {
    return ":" + to_string(val) + "\r\n";
}

string RESPParser::serialize_bulk_string(const string& str) {
    return "$" + to_string(str.length()) + "\r\n" + str + "\r\n";
}

string RESPParser::serialize_null() {
    return "$-1\r\n";
}

string RESPParser::serialize_array(const vector<string>& elements) {
    ostringstream oss;
    oss << "*" << elements.size() << "\r\n";
    for (size_t i = 0; i < elements.size(); ++i) {
        oss << serialize_bulk_string(elements[i]);
    }
    return oss.str();
}

} // namespace redis_clone
