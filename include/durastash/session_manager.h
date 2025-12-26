#pragma once

#include "durastash/storage.h"
#include "durastash/types.h"
#include "durastash/ulid.h"
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

namespace durastash {

/**
 * 프로세스 세션 생명주기 관리자
 * ULID 기반 세션 ID 발행 및 하트비트 관리
 */
class SessionManager {
public:
    explicit SessionManager(IStorage* storage);
    ~SessionManager();

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
     * 현재 세션 ID 반환
     * @return 세션 ID (ULID)
     */
    std::string GetSessionId() const;

    /**
     * 하트비트 업데이트
     * @param group_key 그룹 키
     * @return 성공시 true
     */
    bool UpdateHeartbeat(const std::string& group_key);

    /**
     * 세션이 활성 상태인지 확인
     * @param group_key 그룹 키
     * @param session_id 세션 ID
     * @return 활성시 true
     */
    bool IsSessionActive(const std::string& group_key, const std::string& session_id);

    /**
     * 타임아웃된 세션 정리
     * @param group_key 그룹 키
     * @param timeout_ms 타임아웃 시간 (밀리초)
     * @return 정리된 세션 개수
     */
    size_t CleanupTimeoutSessions(const std::string& group_key, int64_t timeout_ms);

    /**
     * 하트비트 스레드 시작
     * @param interval_ms 하트비트 간격 (밀리초)
     */
    void StartHeartbeatThread(int64_t interval_ms = 5000);

    /**
     * 하트비트 스레드 중지
     */
    void StopHeartbeatThread();

private:
    IStorage* storage_;
    std::string current_session_id_;
    std::atomic<bool> heartbeat_running_;
    std::thread heartbeat_thread_;
    std::mutex mutex_;
    std::string current_group_key_;
    int64_t heartbeat_interval_ms_;

    std::string MakeSessionStateKey(const std::string& group_key, const std::string& session_id);
    std::string MakeSessionLockKey(const std::string& group_key, const std::string& session_id);
    int64_t GetCurrentProcessId();
    void HeartbeatWorker();
};

} // namespace durastash

