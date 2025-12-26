#pragma once

#include "durastash/storage.h"
#include "durastash/types.h"
#include "durastash/ulid.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace durastash {

/**
 * 배치 단위 처리 및 상태 관리자
 * 배치 생성, Load 상태 관리, ACK 처리 담당
 */
class BatchManager {
public:
    explicit BatchManager(IStorage* storage);
    ~BatchManager() = default;

    /**
     * 새 배치 생성
     * @param group_key 그룹 키
     * @param session_id 세션 ID
     * @param sequence_start 시퀀스 시작 번호
     * @param sequence_end 시퀀스 종료 번호
     * @return 배치 ID (ULID)
     */
    std::string CreateBatch(const std::string& group_key,
                           const std::string& session_id,
                           int64_t sequence_start,
                           int64_t sequence_end);

    /**
     * 배치 메타데이터 조회
     * @param group_key 그룹 키
     * @param session_id 세션 ID
     * @param batch_id 배치 ID
     * @param metadata 출력 메타데이터
     * @return 성공시 true
     */
    bool GetBatchMetadata(const std::string& group_key,
                         const std::string& session_id,
                         const std::string& batch_id,
                         BatchMetadata& metadata);

    /**
     * 배치 상태를 Loaded로 변경 (원자적 연산)
     * @param group_key 그룹 키
     * @param session_id 세션 ID
     * @param batch_id 배치 ID
     * @return 성공시 true (이미 Loaded 상태면 false)
     */
    bool MarkBatchAsLoaded(const std::string& group_key,
                          const std::string& session_id,
                          const std::string& batch_id);

    /**
     * 배치 ACK 처리 및 삭제
     * @param group_key 그룹 키
     * @param session_id 세션 ID
     * @param batch_id 배치 ID
     * @return 성공시 true
     */
    bool AcknowledgeBatch(const std::string& group_key,
                         const std::string& session_id,
                         const std::string& batch_id);

    /**
     * Load 가능한 배치 조회 (FIFO 순서)
     * @param group_key 그룹 키
     * @param session_id 세션 ID
     * @param batch_size 배치 크기
     * @param batch_ids 출력 배치 ID 목록
     * @return 조회된 배치 개수
     */
    size_t GetLoadableBatches(const std::string& group_key,
                              const std::string& session_id,
                              size_t batch_size,
                              std::vector<std::string>& batch_ids);

    /**
     * 배치의 모든 데이터 키 생성
     * @param group_key 그룹 키
     * @param session_id 세션 ID
     * @param batch_id 배치 ID
     * @param sequence_start 시퀀스 시작
     * @param sequence_end 시퀀스 종료
     * @param keys 출력 키 목록
     */
    void GenerateDataKeys(const std::string& group_key,
                         const std::string& session_id,
                         const std::string& batch_id,
                         int64_t sequence_start,
                         int64_t sequence_end,
                         std::vector<std::string>& keys);

private:
    IStorage* storage_;
    std::mutex mutex_;

    std::string MakeBatchMetadataKey(const std::string& group_key,
                                     const std::string& session_id,
                                     const std::string& batch_id);
    std::string MakeDataKey(const std::string& group_key,
                           const std::string& session_id,
                           const std::string& batch_id,
                           int64_t sequence_id);
};

} // namespace durastash

