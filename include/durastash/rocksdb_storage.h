#pragma once

#include "durastash/storage.h"
#include <rocksdb/db.h>
#include <rocksdb/write_batch.h>
#include <rocksdb/options.h>
#include <memory>
#include <mutex>

namespace durastash {

/**
 * RocksDB 기반 저장소 구현
 */
class RocksDBStorage : public IStorage {
public:
    RocksDBStorage();
    ~RocksDBStorage() override;

    // IStorage 인터페이스 구현
    bool Initialize(const std::string& db_path) override;
    void Shutdown() override;
    bool Put(const std::string& key, const std::string& value) override;
    bool Get(const std::string& key, std::string& value) override;
    bool Delete(const std::string& key) override;
    bool Exists(const std::string& key) override;
    size_t Scan(const std::string& start_key, 
                const std::string& end_key,
                std::vector<std::string>& keys,
                std::vector<std::string>& values,
                size_t limit = 0) override;
    size_t ScanPrefix(const std::string& prefix,
                      std::vector<std::string>& keys,
                      std::vector<std::string>& values) override;
    bool BeginBatch() override;
    void PutToBatch(const std::string& key, const std::string& value) override;
    void DeleteFromBatch(const std::string& key) override;
    bool CommitBatch() override;
    void RollbackBatch() override;

private:
    std::unique_ptr<rocksdb::DB> db_;
    std::unique_ptr<rocksdb::WriteBatch> current_batch_;
    std::mutex mutex_;
    bool initialized_ = false;

    rocksdb::ReadOptions read_options_;
    rocksdb::WriteOptions write_options_;
};

} // namespace durastash

