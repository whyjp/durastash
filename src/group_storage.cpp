#include "durastash/group_storage.h"
#include "durastash/storage.h"
#include "durastash/errors.h"
#include <algorithm>

namespace durastash {

GroupStorage::GroupStorage(const std::string& db_path)
    : default_batch_size_(100)
    , db_path_(db_path) {
    storage_ = CreateStorage();
    session_manager_ = std::make_unique<SessionManager>(storage_.get());
    batch_manager_ = std::make_unique<BatchManager>(storage_.get());
}

GroupStorage::~GroupStorage() {
    Shutdown();
}

bool GroupStorage::Initialize() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!storage_) {
        return false;
    }

    return storage_->Initialize(db_path_);
}

void GroupStorage::Shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 모든 그룹의 세션 종료
    for (const auto& pair : group_sessions_) {
        session_manager_->TerminateSession(pair.first);
    }
    
    session_manager_->StopHeartbeatThread();
    
    if (storage_) {
        storage_->Shutdown();
    }
    
    group_sessions_.clear();
    group_sequence_counters_.clear();
}

bool GroupStorage::InitializeSession(const std::string& group_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!storage_ || !session_manager_) {
        return false;
    }

    // 세션 초기화
    if (!session_manager_->InitializeSession(group_key)) {
        return false;
    }

    std::string session_id = session_manager_->GetSessionId();
    group_sessions_[group_key] = session_id;
    
    // 하트비트 스레드 시작 (한 번만)
    session_manager_->StartHeartbeatThread(5000);
    
    return true;
}

void GroupStorage::TerminateSession(const std::string& group_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    session_manager_->TerminateSession(group_key);
    group_sessions_.erase(group_key);
    group_sequence_counters_.erase(group_key);
}

bool GroupStorage::Save(const std::string& group_key, const std::string& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!storage_ || !batch_manager_) {
        return false;
    }

    // 세션 확인 및 생성
    std::string session_id = GetOrCreateSession(group_key);
    if (session_id.empty()) {
        return false;
    }

    // 다음 시퀀스 ID 획득
    int64_t sequence_id = GetNextSequenceId(group_key);
    
    // 현재 배치의 시퀀스 범위 계산
    int64_t batch_start = (sequence_id / default_batch_size_) * default_batch_size_;
    int64_t batch_end = batch_start + default_batch_size_ - 1;
    
    // 배치 키 생성 (그룹별 현재 배치 추적용)
    std::string batch_key = group_key + ":" + std::to_string(batch_start);
    
    // 배치가 새로 시작되는 경우 배치 생성
    std::string batch_id;
    auto batch_it = group_current_batch_ids_.find(batch_key);
    if (batch_it == group_current_batch_ids_.end()) {
        batch_id = batch_manager_->CreateBatch(group_key, session_id, 
                                              batch_start, batch_end);
        if (batch_id.empty()) {
            return false;
        }
        group_current_batch_ids_[batch_key] = batch_id;
    } else {
        batch_id = batch_it->second;
    }

    // 데이터 키 생성 및 저장
    std::vector<std::string> data_keys;
    batch_manager_->GenerateDataKeys(group_key, session_id, batch_id,
                                     sequence_id, sequence_id, data_keys);
    
    if (data_keys.empty()) {
        return false;
    }

    return storage_->Put(data_keys[0], data);
}

std::vector<BatchLoadResult> GroupStorage::LoadBatch(const std::string& group_key, size_t batch_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<BatchLoadResult> results;
    
    if (!storage_ || !batch_manager_) {
        return results;
    }

    // 세션 확인
    auto it = group_sessions_.find(group_key);
    if (it == group_sessions_.end()) {
        return results;
    }
    
    std::string session_id = it->second;

    // Load 가능한 배치 조회
    std::vector<std::string> batch_ids;
    size_t count = batch_manager_->GetLoadableBatches(group_key, session_id, 
                                                      batch_size, batch_ids);
    
    if (count == 0) {
        return results;
    }

    // 각 배치를 Load
    for (const auto& batch_id : batch_ids) {
        // 배치를 Loaded 상태로 변경 (원자적 연산)
        if (!batch_manager_->MarkBatchAsLoaded(group_key, session_id, batch_id)) {
            continue;  // 이미 Loaded 상태면 스킵
        }

        // 배치 데이터 로드
        BatchLoadResult result;
        if (LoadBatchData(group_key, session_id, batch_id, result)) {
            results.push_back(result);
        }
    }

    return results;
}

bool GroupStorage::AcknowledgeBatch(const std::string& group_key, const std::string& batch_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!batch_manager_) {
        return false;
    }

    // 세션 확인
    auto it = group_sessions_.find(group_key);
    if (it == group_sessions_.end()) {
        return false;
    }
    
    std::string session_id = it->second;
    
    return batch_manager_->AcknowledgeBatch(group_key, session_id, batch_id);
}

bool GroupStorage::ResaveBatch(const std::string& group_key,
                               const std::string& batch_id,
                               const std::vector<std::string>& remaining_data) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!storage_ || !batch_manager_) {
        return false;
    }

    // 세션 확인
    auto it = group_sessions_.find(group_key);
    if (it == group_sessions_.end()) {
        return false;
    }
    
    std::string session_id = it->second;

    // 원본 배치 메타데이터 조회
    BatchMetadata original_metadata;
    if (!batch_manager_->GetBatchMetadata(group_key, session_id, batch_id, original_metadata)) {
        return false;
    }

    // 원본 배치가 Loaded 상태인지 확인
    if (original_metadata.GetStatus() != BatchStatus::LOADED) {
        return false;
    }

    // 남은 데이터가 없으면 원본 배치만 ACK
    if (remaining_data.empty()) {
        return batch_manager_->AcknowledgeBatch(group_key, session_id, batch_id);
    }

    // 새 배치 생성
    int64_t new_sequence_start = GetNextSequenceId(group_key);
    int64_t new_sequence_end = new_sequence_start + remaining_data.size() - 1;
    
    std::string new_batch_id = batch_manager_->CreateBatch(group_key, session_id,
                                                           new_sequence_start, new_sequence_end);
    if (new_batch_id.empty()) {
        return false;
    }

    // 배치 쓰기 시작
    if (!storage_->BeginBatch()) {
        return false;
    }

    // 새 배치에 데이터 저장
    std::vector<std::string> new_data_keys;
    batch_manager_->GenerateDataKeys(group_key, session_id, new_batch_id,
                                    new_sequence_start, new_sequence_end, new_data_keys);
    
    for (size_t i = 0; i < remaining_data.size() && i < new_data_keys.size(); ++i) {
        storage_->PutToBatch(new_data_keys[i], remaining_data[i]);
    }

    // 원본 배치 ACK (삭제)
    batch_manager_->AcknowledgeBatch(group_key, session_id, batch_id);

    // 배치 커밋
    return storage_->CommitBatch();
}

std::string GroupStorage::GetSessionId(const std::string& group_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = group_sessions_.find(group_key);
    if (it != group_sessions_.end()) {
        return it->second;
    }
    
    return "";
}

void GroupStorage::SetBatchSize(size_t batch_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    default_batch_size_ = batch_size;
}


int64_t GroupStorage::GetNextSequenceId(const std::string& group_key) {
    auto it = group_sequence_counters_.find(group_key);
    if (it == group_sequence_counters_.end()) {
        group_sequence_counters_[group_key] = 0;
        return 0;
    }
    
    int64_t next = it->second + 1;
    it->second = next;
    return next;
}

std::string GroupStorage::GetOrCreateSession(const std::string& group_key) {
    auto it = group_sessions_.find(group_key);
    if (it != group_sessions_.end()) {
        return it->second;
    }
    
    // 세션 초기화
    if (InitializeSession(group_key)) {
        return group_sessions_[group_key];
    }
    
    return "";
}

bool GroupStorage::LoadBatchData(const std::string& group_key,
                                const std::string& session_id,
                                const std::string& batch_id,
                                BatchLoadResult& result) {
    // 배치 메타데이터 조회
    BatchMetadata metadata;
    if (!batch_manager_->GetBatchMetadata(group_key, session_id, batch_id, metadata)) {
        return false;
    }

    result.batch_id = batch_id;
    result.sequence_start = metadata.GetSequenceStart();
    result.sequence_end = metadata.GetSequenceEnd();

    // 배치의 모든 데이터 키 생성
    std::vector<std::string> data_keys;
    batch_manager_->GenerateDataKeys(group_key, session_id, batch_id,
                                    metadata.GetSequenceStart(),
                                    metadata.GetSequenceEnd(),
                                    data_keys);

    // 데이터 로드
    result.data.clear();
    for (const auto& key : data_keys) {
        std::string value;
        if (storage_->Get(key, value)) {
            result.data.push_back(value);
        }
    }

    return true;
}

} // namespace durastash

