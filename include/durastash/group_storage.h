#pragma once

#include "durastash/storage.h"
#include "durastash/session_manager.h"
#include "durastash/batch_manager.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace durastash {

/**
 * 배치 로드 결과
 */
struct BatchLoadResult {
    std::string batch_id;              // 배치 ID
    std::vector<std::string> data;      // 데이터 목록
    int64_t sequence_start;             // 시퀀스 시작
    int64_t sequence_end;               // 시퀀스 종료
};

/**
 * 그룹별 저장소 관리자
 * 그룹 키 기반 데이터 분리, FIFO 순서 보장, 배치 단위 처리
 */
class GroupStorage {
public:
    /**
     * 생성자
     * @param db_path 데이터베이스 경로
     */
    explicit GroupStorage(const std::string& db_path);
    
    ~GroupStorage();

    /**
     * 저장소 초기화
     * @return 성공시 true
     */
    bool Initialize();

    /**
     * 저장소 종료
     */
    void Shutdown();

    /**
     * 세션 초기화
     * @param group_key 그룹 키
     * @return 성공시 true
     */
    bool InitializeSession(const std::string& group_key);

    /**
     * 세션 종료 및 정리
     * @param group_key 그룹 키
     */
    void TerminateSession(const std::string& group_key);

    /**
     * 그룹별 데이터 저장
     * @param group_key 그룹 키
     * @param data 저장할 데이터
     * @return 성공시 true
     */
    bool Save(const std::string& group_key, const std::string& data);

    /**
     * 배치 단위 로드 (FIFO, 한번 Load된 배치는 재Load 불가)
     * @param group_key 그룹 키
     * @param batch_size 배치 크기
     * @return 배치 로드 결과 목록
     */
    std::vector<BatchLoadResult> LoadBatch(const std::string& group_key, size_t batch_size);

    /**
     * 배치 ACK 및 삭제
     * @param group_key 그룹 키
     * @param batch_id 배치 ID
     * @return 성공시 true
     */
    bool AcknowledgeBatch(const std::string& group_key, const std::string& batch_id);

    /**
     * 부분 처리된 배치의 Resave
     * @param group_key 그룹 키
     * @param batch_id 원본 배치 ID
     * @param remaining_data 남은 데이터 목록
     * @return 성공시 true
     */
    bool ResaveBatch(const std::string& group_key,
                    const std::string& batch_id,
                    const std::vector<std::string>& remaining_data);

    /**
     * 현재 세션 ID 반환
     * @param group_key 그룹 키
     * @return 세션 ID
     */
    std::string GetSessionId(const std::string& group_key);

    /**
     * 배치 크기 설정
     * @param batch_size 배치 크기
     */
    void SetBatchSize(size_t batch_size);

    /**
     * 배치 크기 반환
     * @return 배치 크기
     */
    size_t GetBatchSize() const {
        return default_batch_size_;
    }

private:
    std::string db_path_;
    std::unique_ptr<IStorage> storage_;
    std::unique_ptr<SessionManager> session_manager_;
    std::unique_ptr<BatchManager> batch_manager_;
    
    std::mutex mutex_;
    std::unordered_map<std::string, int64_t> group_sequence_counters_;
    std::unordered_map<std::string, std::string> group_sessions_;
    std::unordered_map<std::string, std::string> group_current_batch_ids_;
    size_t default_batch_size_;

    int64_t GetNextSequenceId(const std::string& group_key);
    std::string GetOrCreateSession(const std::string& group_key);
    bool LoadBatchData(const std::string& group_key,
                      const std::string& session_id,
                      const std::string& batch_id,
                      BatchLoadResult& result);
};

} // namespace durastash

