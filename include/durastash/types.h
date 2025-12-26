#pragma once

#include <string>
#include <cstdint>
#include "jsonable/Jsonable.hpp"

namespace durastash {

/**
 * 배치 상태 열거형
 */
enum class BatchStatus {
    PENDING,      // 대기 중
    LOADED,       // 로드됨 (재로드 불가)
    ACKNOWLEDGED  // 확인됨 (삭제 예정)
};

/**
 * 세션 상태 열거형
 */
enum class SessionStatus {
    ACTIVE,      // 활성
    TERMINATED   // 종료됨
};

/**
 * 배치 메타데이터
 * jsonable을 상속받아 JSON 직렬화/역직렬화 지원
 */
class BatchMetadata : public json::Jsonable {
public:
    BatchMetadata() = default;
    ~BatchMetadata() = default;

    // Getter/Setter
    const std::string& GetBatchId() const { return batch_id_; }
    void SetBatchId(const std::string& batch_id) { batch_id_ = batch_id; }

    int64_t GetSequenceStart() const { return sequence_start_; }
    void SetSequenceStart(int64_t start) { sequence_start_ = start; }

    int64_t GetSequenceEnd() const { return sequence_end_; }
    void SetSequenceEnd(int64_t end) { sequence_end_ = end; }

    BatchStatus GetStatus() const { return status_; }
    void SetStatus(BatchStatus status) { status_ = status; }

    int64_t GetCreatedAt() const { return created_at_; }
    void SetCreatedAt(int64_t timestamp) { created_at_ = timestamp; }

    int64_t GetLoadedAt() const { return loaded_at_; }
    void SetLoadedAt(int64_t timestamp) { loaded_at_ = timestamp; }

    // jsonable 인터페이스 구현
    void saveToJson() override {
        setString("batch_id", batch_id_);
        setInt64("sequence_start", sequence_start_);
        setInt64("sequence_end", sequence_end_);
        setString("status", StatusToString(status_));
        setInt64("created_at", created_at_);
        if (loaded_at_ > 0) {
            setInt64("loaded_at", loaded_at_);
        }
    }

    void loadFromJson() override {
        batch_id_ = getString("batch_id");
        sequence_start_ = getInt64("sequence_start");
        sequence_end_ = getInt64("sequence_end");
        status_ = StringToStatus(getString("status"));
        created_at_ = getInt64("created_at");
        if (hasKey("loaded_at")) {
            loaded_at_ = getInt64("loaded_at");
        } else {
            loaded_at_ = 0;
        }
    }

private:
    std::string batch_id_;      // ULID
    int64_t sequence_start_ = 0;
    int64_t sequence_end_ = 0;
    BatchStatus status_ = BatchStatus::PENDING;
    int64_t created_at_ = 0;
    int64_t loaded_at_ = 0;     // 0이면 미설정

    static std::string StatusToString(BatchStatus status) {
        switch (status) {
            case BatchStatus::PENDING: return "pending";
            case BatchStatus::LOADED: return "loaded";
            case BatchStatus::ACKNOWLEDGED: return "acknowledged";
            default: return "pending";
        }
    }

    static BatchStatus StringToStatus(const std::string& str) {
        if (str == "pending") return BatchStatus::PENDING;
        if (str == "loaded") return BatchStatus::LOADED;
        if (str == "acknowledged") return BatchStatus::ACKNOWLEDGED;
        return BatchStatus::PENDING;
    }
};

/**
 * 세션 상태 정보
 * jsonable을 상속받아 JSON 직렬화/역직렬화 지원
 */
class SessionState : public json::Jsonable {
public:
    SessionState() = default;
    ~SessionState() = default;

    // Getter/Setter
    const std::string& GetSessionId() const { return session_id_; }
    void SetSessionId(const std::string& session_id) { session_id_ = session_id; }

    int64_t GetProcessId() const { return process_id_; }
    void SetProcessId(int64_t pid) { process_id_ = pid; }

    int64_t GetStartedAt() const { return started_at_; }
    void SetStartedAt(int64_t timestamp) { started_at_ = timestamp; }

    int64_t GetLastHeartbeat() const { return last_heartbeat_; }
    void SetLastHeartbeat(int64_t timestamp) { last_heartbeat_ = timestamp; }

    SessionStatus GetStatus() const { return status_; }
    void SetStatus(SessionStatus status) { status_ = status; }

    // jsonable 인터페이스 구현
    void saveToJson() override {
        setString("session_id", session_id_);
        setInt64("process_id", process_id_);
        setInt64("started_at", started_at_);
        setInt64("last_heartbeat", last_heartbeat_);
        setString("status", StatusToString(status_));
    }

    void loadFromJson() override {
        session_id_ = getString("session_id");
        process_id_ = getInt64("process_id");
        started_at_ = getInt64("started_at");
        last_heartbeat_ = getInt64("last_heartbeat");
        status_ = StringToStatus(getString("status"));
    }

private:
    std::string session_id_;    // ULID
    int64_t process_id_ = 0;
    int64_t started_at_ = 0;
    int64_t last_heartbeat_ = 0;
    SessionStatus status_ = SessionStatus::ACTIVE;

    static std::string StatusToString(SessionStatus status) {
        switch (status) {
            case SessionStatus::ACTIVE: return "active";
            case SessionStatus::TERMINATED: return "terminated";
            default: return "active";
        }
    }

    static SessionStatus StringToStatus(const std::string& str) {
        if (str == "active") return SessionStatus::ACTIVE;
        if (str == "terminated") return SessionStatus::TERMINATED;
        return SessionStatus::ACTIVE;
    }
};

} // namespace durastash

