#include "durastash/batch_manager.h"
#include "durastash/errors.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace durastash {

BatchManager::BatchManager(IStorage* storage)
    : storage_(storage) {
}

std::string BatchManager::CreateBatch(const std::string& group_key,
                                     const std::string& session_id,
                                     int64_t sequence_start,
                                     int64_t sequence_end) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!storage_) {
        return "";
    }

    // 새 배치 ID 생성
    std::string batch_id = ULID::Generate();

    // 배치 메타데이터 생성
    BatchMetadata metadata;
    metadata.SetBatchId(batch_id);
    metadata.SetSequenceStart(sequence_start);
    metadata.SetSequenceEnd(sequence_end);
    metadata.SetStatus(BatchStatus::PENDING);
    metadata.SetCreatedAt(ULID::Now());
    metadata.SetLoadedAt(0);

    // JSON 직렬화
    std::string json_str = metadata.toJson();
    
    // 저장소에 저장
    std::string key = MakeBatchMetadataKey(group_key, session_id, batch_id);
    if (!storage_->Put(key, json_str)) {
        return "";
    }

    return batch_id;
}

bool BatchManager::GetBatchMetadata(const std::string& group_key,
                                    const std::string& session_id,
                                    const std::string& batch_id,
                                    BatchMetadata& metadata) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!storage_) {
        return false;
    }

    std::string key = MakeBatchMetadataKey(group_key, session_id, batch_id);
    std::string json_str;
    
    if (!storage_->Get(key, json_str)) {
        return false;
    }

    metadata.fromJson(json_str);
    return true;
}

bool BatchManager::MarkBatchAsLoaded(const std::string& group_key,
                                    const std::string& session_id,
                                    const std::string& batch_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!storage_) {
        return false;
    }

    std::string key = MakeBatchMetadataKey(group_key, session_id, batch_id);
    std::string json_str;
    
    if (!storage_->Get(key, json_str)) {
        throw BatchNotFoundException(batch_id);
    }

    BatchMetadata metadata;
    try {
        metadata.fromJson(json_str);
    } catch (...) {
        throw CorruptedBatchException(batch_id);
    }
    
    // 이미 Loaded 상태면 실패
    if (metadata.GetStatus() == BatchStatus::LOADED) {
        return false;
    }

    // 상태를 Loaded로 변경
    metadata.SetStatus(BatchStatus::LOADED);
    metadata.SetLoadedAt(ULID::Now());
    
    std::string updated_json = metadata.toJson();
    return storage_->Put(key, updated_json);
}

bool BatchManager::AcknowledgeBatch(const std::string& group_key,
                                    const std::string& session_id,
                                    const std::string& batch_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!storage_) {
        return false;
    }

    // 배치 메타데이터 조회 (이미 mutex 잠금 상태이므로 직접 조회)
    std::string key = MakeBatchMetadataKey(group_key, session_id, batch_id);
    std::string json_str;
    
    if (!storage_->Get(key, json_str)) {
        return false;
    }
    
    BatchMetadata metadata;
    try {
        metadata.fromJson(json_str);
    } catch (...) {
        return false;
    }

    // 배치 쓰기 시작
    if (!storage_->BeginBatch()) {
        return false;
    }

    // 배치 메타데이터 삭제
    std::string metadata_key = MakeBatchMetadataKey(group_key, session_id, batch_id);
    storage_->DeleteFromBatch(metadata_key);

    // 배치의 모든 데이터 키 삭제
    std::vector<std::string> data_keys;
    GenerateDataKeys(group_key, session_id, batch_id,
                    metadata.GetSequenceStart(),
                    metadata.GetSequenceEnd(),
                    data_keys);

    for (const auto& data_key : data_keys) {
        storage_->DeleteFromBatch(data_key);
    }

    // 배치 커밋
    return storage_->CommitBatch();
}

size_t BatchManager::GetLoadableBatches(const std::string& group_key,
                                       const std::string& session_id,
                                       size_t batch_size,
                                       std::vector<std::string>& batch_ids) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    batch_ids.clear();
    
    if (!storage_) {
        return 0;
    }

    // 세션의 모든 배치 메타데이터 조회
    std::string prefix = group_key + ":" + session_id + ":batch:";
    std::vector<std::string> keys;
    std::vector<std::string> values;
    
    storage_->ScanPrefix(prefix, keys, values);

    // PENDING 상태인 배치만 필터링 및 정렬
    std::vector<std::pair<std::string, int64_t>> pending_batches;
    
    for (size_t i = 0; i < keys.size(); ++i) {
        BatchMetadata metadata;
        metadata.fromJson(values[i]);
        
        if (metadata.GetStatus() == BatchStatus::PENDING) {
            pending_batches.push_back({metadata.GetBatchId(), metadata.GetSequenceStart()});
        }
    }

    // 시퀀스 시작 번호로 정렬 (FIFO)
    std::sort(pending_batches.begin(), pending_batches.end(),
              [](const auto& a, const auto& b) {
                  return a.second < b.second;
              });

    // 요청된 개수만큼 반환
    size_t count = std::min(batch_size, pending_batches.size());
    for (size_t i = 0; i < count; ++i) {
        batch_ids.push_back(pending_batches[i].first);
    }

    return batch_ids.size();
}

void BatchManager::GenerateDataKeys(const std::string& group_key,
                                   const std::string& session_id,
                                   const std::string& batch_id,
                                   int64_t sequence_start,
                                   int64_t sequence_end,
                                   std::vector<std::string>& keys) {
    keys.clear();
    
    for (int64_t seq = sequence_start; seq <= sequence_end; ++seq) {
        std::string key = MakeDataKey(group_key, session_id, batch_id, seq);
        keys.push_back(key);
    }
}

std::string BatchManager::MakeBatchMetadataKey(const std::string& group_key,
                                              const std::string& session_id,
                                              const std::string& batch_id) {
    // public 메서드로 변경되었으므로 mutex 잠금 불필요 (호출자가 이미 잠금을 가지고 있음)
    return group_key + ":" + session_id + ":batch:" + batch_id;
}

std::string BatchManager::MakeDataKey(const std::string& group_key,
                                     const std::string& session_id,
                                     const std::string& batch_id,
                                     int64_t sequence_id) {
    std::ostringstream oss;
    oss << group_key << ":" << session_id << ":" << batch_id << ":";
    oss << std::setfill('0') << std::setw(20) << sequence_id;
    return oss.str();
}

std::string BatchManager::FindBatchIdBySequenceId(const std::string& group_key,
                                                 const std::string& session_id,
                                                 int64_t sequence_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!storage_) {
        return "";
    }

    // 세션의 모든 배치 메타데이터 조회
    std::string prefix = group_key + ":" + session_id + ":batch:";
    std::vector<std::string> keys;
    std::vector<std::string> values;
    
    storage_->ScanPrefix(prefix, keys, values);

    // sequence_id가 포함된 배치 찾기
    for (size_t i = 0; i < keys.size(); ++i) {
        BatchMetadata metadata;
        try {
            metadata.fromJson(values[i]);
        } catch (...) {
            continue;
        }
        
        // sequence_id가 이 배치의 범위에 있는지 확인
        if (sequence_id >= metadata.GetSequenceStart() && 
            sequence_id <= metadata.GetSequenceEnd()) {
            return metadata.GetBatchId();
        }
    }

    return "";
}

std::string BatchManager::MakeDataKeyBySequenceId(const std::string& group_key,
                                                 const std::string& session_id,
                                                 int64_t sequence_id) {
    // sequence_id가 포함된 배치 찾기
    std::string batch_id = FindBatchIdBySequenceId(group_key, session_id, sequence_id);
    if (batch_id.empty()) {
        return "";
    }
    
    // 데이터 키 생성
    return MakeDataKey(group_key, session_id, batch_id, sequence_id);
}

} // namespace durastash

