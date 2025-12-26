#include "durastash/session_manager.h"
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/types.h>
#endif

namespace durastash {

SessionManager::SessionManager(IStorage* storage)
    : storage_(storage)
    , heartbeat_running_(false)
    , heartbeat_interval_ms_(5000) {
}

SessionManager::~SessionManager() {
    StopHeartbeatThread();
}

bool SessionManager::InitializeSession(const std::string& group_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!storage_) {
        return false;
    }

    // 새 세션 ID 생성
    current_session_id_ = ULID::Generate();
    current_group_key_ = group_key;

    // 세션 상태 생성
    SessionState state;
    state.SetSessionId(current_session_id_);
    state.SetProcessId(GetCurrentProcessId());
    state.SetStartedAt(ULID::Now());
    state.SetLastHeartbeat(ULID::Now());
    state.SetStatus(SessionStatus::ACTIVE);

    // JSON 직렬화
    std::string json_str = state.toJson();
    
    // 저장소에 저장
    std::string key = MakeSessionStateKey(group_key, current_session_id_);
    if (!storage_->Put(key, json_str)) {
        return false;
    }

    return true;
}

void SessionManager::TerminateSession(const std::string& group_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (current_session_id_.empty() || !storage_) {
        return;
    }

    std::string key = MakeSessionStateKey(group_key, current_session_id_);
    std::string json_str;
    
    if (storage_->Get(key, json_str)) {
        SessionState state;
        state.fromJson(json_str);
        state.SetStatus(SessionStatus::TERMINATED);
        state.SetLastHeartbeat(ULID::Now());
        
        std::string updated_json = state.toJson();
        storage_->Put(key, updated_json);
    }

    current_session_id_.clear();
    current_group_key_.clear();
}

std::string SessionManager::GetSessionId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_session_id_;
}

bool SessionManager::UpdateHeartbeat(const std::string& group_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (current_session_id_.empty() || !storage_) {
        return false;
    }

    std::string key = MakeSessionStateKey(group_key, current_session_id_);
    std::string json_str;
    
    if (!storage_->Get(key, json_str)) {
        return false;
    }

    SessionState state;
    state.fromJson(json_str);
    state.SetLastHeartbeat(ULID::Now());
    
    std::string updated_json = state.toJson();
    return storage_->Put(key, updated_json);
}

bool SessionManager::IsSessionActive(const std::string& group_key, const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!storage_) {
        return false;
    }

    std::string key = MakeSessionStateKey(group_key, session_id);
    std::string json_str;
    
    if (!storage_->Get(key, json_str)) {
        return false;
    }

    SessionState state;
    state.fromJson(json_str);
    
    return state.GetStatus() == SessionStatus::ACTIVE;
}

size_t SessionManager::CleanupTimeoutSessions(const std::string& group_key, int64_t timeout_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!storage_) {
        return 0;
    }

    std::string prefix = group_key + ":";
    std::vector<std::string> keys;
    std::vector<std::string> values;
    
    size_t count = storage_->ScanPrefix(prefix, keys, values);
    
    int64_t current_time = ULID::Now();
    size_t cleaned = 0;

    for (size_t i = 0; i < keys.size(); ++i) {
        // 세션 상태 키만 처리
        if (keys[i].find(":state") == std::string::npos) {
            continue;
        }

        SessionState state;
        state.fromJson(values[i]);
        
        // 타임아웃 확인
        if (state.GetStatus() == SessionStatus::ACTIVE) {
            int64_t elapsed = current_time - state.GetLastHeartbeat();
            if (elapsed > timeout_ms) {
                // 세션을 종료 상태로 변경
                state.SetStatus(SessionStatus::TERMINATED);
                state.SetLastHeartbeat(current_time);
                
                std::string updated_json = state.toJson();
                storage_->Put(keys[i], updated_json);
                cleaned++;
            }
        }
    }

    return cleaned;
}

void SessionManager::StartHeartbeatThread(int64_t interval_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (heartbeat_running_) {
        return;
    }

    heartbeat_interval_ms_ = interval_ms;
    heartbeat_running_ = true;
    
    heartbeat_thread_ = std::thread(&SessionManager::HeartbeatWorker, this);
}

void SessionManager::StopHeartbeatThread() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!heartbeat_running_) {
            return;
        }
        heartbeat_running_ = false;
    }

    if (heartbeat_thread_.joinable()) {
        heartbeat_thread_.join();
    }
}

void SessionManager::HeartbeatWorker() {
    while (heartbeat_running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeat_interval_ms_));
        
        if (!heartbeat_running_) {
            break;
        }

        std::string group_key;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            group_key = current_group_key_;
        }

        if (!group_key.empty()) {
            UpdateHeartbeat(group_key);
        }
    }
}

std::string SessionManager::MakeSessionStateKey(const std::string& group_key, const std::string& session_id) {
    return group_key + ":" + session_id + ":state";
}

std::string SessionManager::MakeSessionLockKey(const std::string& group_key, const std::string& session_id) {
    return group_key + ":" + session_id + ":lock";
}

int64_t SessionManager::GetCurrentProcessId() {
#ifdef _WIN32
    return static_cast<int64_t>(_getpid());
#else
    return static_cast<int64_t>(getpid());
#endif
}

} // namespace durastash

