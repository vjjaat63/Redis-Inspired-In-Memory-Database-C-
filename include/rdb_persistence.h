#ifndef RDB_PERSISTENCE_H
#define RDB_PERSISTENCE_H

#include "redis_clone.h"
#include "data_store.h"
#include <string>
#include <memory>

namespace redis_clone {

class RDBPersistence {
public:
    explicit RDBPersistence(std::shared_ptr<DataStore> data_store);
    
    // Save database to RDB file
    bool save(const std::string& filename);
    
    // Load database from RDB file
    bool load(const std::string& filename);

private:
    std::shared_ptr<DataStore> data_store_;
    
    // RDB format constants
    static const uint8_t RDB_VERSION = 9;
    static const std::string REDIS_MAGIC;
    
    // RDB opcodes
    enum class Opcode : uint8_t {
        EOF_MARK = 0xFF,
        SELECTDB = 0xFE,
        RESIZEDB = 0xFB,
        EXPIRETIME_MS = 0xFC,
        EXPIRETIME = 0xFD,
        AUX = 0xFA
    };
    
    // RDB value types
    enum class ValueType : uint8_t {
        STRING = 0,
        LIST = 1,
        SET = 2,
        HASH = 3,
        ZSET = 4,
        HASH_ZIPMAP = 9,
        LIST_ZIPLIST = 10,
        SET_INTSET = 11,
        ZSET_ZIPLIST = 12,
        HASH_ZIPLIST = 13
    };
    
    // Helper functions
    bool write_string(std::ofstream& file, const std::string& str);
    bool write_integer(std::ofstream& file, int64_t value);
    bool write_length(std::ofstream& file, uint32_t length);
    std::optional<std::string> read_string(std::ifstream& file);
    std::optional<int64_t> read_integer(std::ifstream& file);
    std::optional<uint32_t> read_length(std::ifstream& file);
};

} // namespace redis_clone

#endif // RDB_PERSISTENCE_H
