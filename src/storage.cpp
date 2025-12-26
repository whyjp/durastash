#include "durastash/storage.h"
#include "durastash/rocksdb_storage.h"

namespace durastash {

std::unique_ptr<IStorage> CreateStorage() {
    return std::make_unique<RocksDBStorage>();
}

} // namespace durastash

