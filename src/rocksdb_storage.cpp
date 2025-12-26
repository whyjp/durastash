#include "durastash/rocksdb_storage.h"
#include <rocksdb/iterator.h>
#include <rocksdb/status.h>
#include <algorithm>

namespace durastash {

RocksDBStorage::RocksDBStorage() {
    write_options_.sync = true;  // 고가용성을 위한 동기 쓰기
}

RocksDBStorage::~RocksDBStorage() {
    Shutdown();
}

bool RocksDBStorage::Initialize(const std::string& db_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return true;
    }

    rocksdb::Options options;
    options.create_if_missing = true;
    options.error_if_exists = false;
    
    // 성능 최적화 옵션
    options.IncreaseParallelism();
    options.OptimizeLevelStyleCompaction();
    
    // 고가용성 옵션
    options.paranoid_checks = true;
    options.write_buffer_size = 64 * 1024 * 1024;  // 64MB
    options.max_write_buffer_number = 3;
    options.min_write_buffer_number_to_merge = 1;

    rocksdb::Status status = rocksdb::DB::Open(options, db_path, &db_);
    
    if (!status.ok()) {
        return false;
    }

    initialized_ = true;
    return true;
}

void RocksDBStorage::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (current_batch_) {
        RollbackBatch();
    }
    
    if (db_) {
        db_.reset();
    }
    
    initialized_ = false;
}

bool RocksDBStorage::Put(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !db_) {
        return false;
    }

    rocksdb::Status status = db_->Put(write_options_, key, value);
    return status.ok();
}

bool RocksDBStorage::Get(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !db_) {
        return false;
    }

    rocksdb::Status status = db_->Get(read_options_, key, &value);
    return status.ok();
}

bool RocksDBStorage::Delete(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !db_) {
        return false;
    }

    rocksdb::Status status = db_->Delete(write_options_, key);
    return status.ok();
}

bool RocksDBStorage::Exists(const std::string& key) {
    std::string value;
    return Get(key, value);
}

size_t RocksDBStorage::Scan(const std::string& start_key, 
                            const std::string& end_key,
                            std::vector<std::string>& keys,
                            std::vector<std::string>& values,
                            size_t limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    keys.clear();
    values.clear();
    
    if (!initialized_ || !db_) {
        return 0;
    }

    std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(read_options_));
    
    size_t count = 0;
    for (it->Seek(start_key); 
         it->Valid() && it->key().ToString() <= end_key; 
         it->Next()) {
        
        keys.push_back(it->key().ToString());
        values.push_back(it->value().ToString());
        count++;
        
        if (limit > 0 && count >= limit) {
            break;
        }
    }
    
    return count;
}

size_t RocksDBStorage::ScanPrefix(const std::string& prefix,
                                  std::vector<std::string>& keys,
                                  std::vector<std::string>& values) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    keys.clear();
    values.clear();
    
    if (!initialized_ || !db_) {
        return 0;
    }

    std::unique_ptr<rocksdb::Iterator> it(db_->NewIterator(read_options_));
    
    size_t count = 0;
    std::string prefix_end = prefix;
    // 접두사의 마지막 문자를 증가시켜 범위 종료점 생성
    if (!prefix_end.empty()) {
        prefix_end.back() = static_cast<char>(prefix_end.back() + 1);
    }
    
    for (it->Seek(prefix); 
         it->Valid() && it->key().starts_with(prefix); 
         it->Next()) {
        
        keys.push_back(it->key().ToString());
        values.push_back(it->value().ToString());
        count++;
    }
    
    return count;
}

bool RocksDBStorage::BeginBatch() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !db_) {
        return false;
    }
    
    if (current_batch_) {
        return false;  // 이미 배치가 시작됨
    }
    
    current_batch_ = std::make_unique<rocksdb::WriteBatch>();
    return true;
}

void RocksDBStorage::PutToBatch(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (current_batch_) {
        current_batch_->Put(key, value);
    }
}

void RocksDBStorage::DeleteFromBatch(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (current_batch_) {
        current_batch_->Delete(key);
    }
}

bool RocksDBStorage::CommitBatch() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_ || !db_ || !current_batch_) {
        return false;
    }

    rocksdb::Status status = db_->Write(write_options_, current_batch_.get());
    current_batch_.reset();
    
    return status.ok();
}

void RocksDBStorage::RollbackBatch() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    current_batch_.reset();
}

} // namespace durastash

