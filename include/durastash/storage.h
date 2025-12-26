#pragma once

#include <string>
#include <vector>
#include <memory>

namespace durastash {

/**
 * 저장소 인터페이스 (DIP 준수)
 * 다양한 저장소 구현체를 지원하기 위한 추상화
 */
class IStorage {
public:
    virtual ~IStorage() = default;

    /**
     * 저장소 초기화
     * @param db_path 데이터베이스 경로
     * @return 성공시 true
     */
    virtual bool Initialize(const std::string& db_path) = 0;

    /**
     * 저장소 종료
     */
    virtual void Shutdown() = 0;

    /**
     * 키-값 저장
     * @param key 키
     * @param value 값
     * @return 성공시 true
     */
    virtual bool Put(const std::string& key, const std::string& value) = 0;

    /**
     * 키-값 조회
     * @param key 키
     * @param value 출력 값
     * @return 성공시 true
     */
    virtual bool Get(const std::string& key, std::string& value) = 0;

    /**
     * 키 삭제
     * @param key 키
     * @return 성공시 true
     */
    virtual bool Delete(const std::string& key) = 0;

    /**
     * 키 존재 여부 확인
     * @param key 키
     * @return 존재시 true
     */
    virtual bool Exists(const std::string& key) = 0;

    /**
     * 범위 스캔
     * @param start_key 시작 키 (포함)
     * @param end_key 종료 키 (포함)
     * @param keys 출력 키 목록
     * @param values 출력 값 목록
     * @param limit 최대 개수 (0이면 제한 없음)
     * @return 조회된 개수
     */
    virtual size_t Scan(const std::string& start_key, 
                       const std::string& end_key,
                       std::vector<std::string>& keys,
                       std::vector<std::string>& values,
                       size_t limit = 0) = 0;

    /**
     * 접두사로 시작하는 모든 키 조회
     * @param prefix 접두사
     * @param keys 출력 키 목록
     * @param values 출력 값 목록
     * @return 조회된 개수
     */
    virtual size_t ScanPrefix(const std::string& prefix,
                              std::vector<std::string>& keys,
                              std::vector<std::string>& values) = 0;

    /**
     * 배치 쓰기 시작
     * @return 성공시 true
     */
    virtual bool BeginBatch() = 0;

    /**
     * 배치에 키-값 추가
     * @param key 키
     * @param value 값
     */
    virtual void PutToBatch(const std::string& key, const std::string& value) = 0;

    /**
     * 배치에서 키 삭제 추가
     * @param key 키
     */
    virtual void DeleteFromBatch(const std::string& key) = 0;

    /**
     * 배치 쓰기 커밋
     * @return 성공시 true
     */
    virtual bool CommitBatch() = 0;

    /**
     * 배치 쓰기 롤백
     */
    virtual void RollbackBatch() = 0;
};

/**
 * 저장소 팩토리 함수
 */
std::unique_ptr<IStorage> CreateStorage();

} // namespace durastash

