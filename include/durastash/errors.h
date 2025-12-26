#pragma once

#include <string>
#include <stdexcept>

namespace durastash {

/**
 * DuraStash 예외 기본 클래스
 */
class DuraStashException : public std::runtime_error {
public:
    explicit DuraStashException(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * 저장소 초기화 실패 예외
 */
class StorageInitializationException : public DuraStashException {
public:
    explicit StorageInitializationException(const std::string& message)
        : DuraStashException("Storage initialization failed: " + message) {}
};

/**
 * 세션 초기화 실패 예외
 */
class SessionInitializationException : public DuraStashException {
public:
    explicit SessionInitializationException(const std::string& message)
        : DuraStashException("Session initialization failed: " + message) {}
};

/**
 * 배치 처리 예외
 */
class BatchException : public DuraStashException {
public:
    explicit BatchException(const std::string& message)
        : DuraStashException("Batch operation failed: " + message) {}
};

/**
 * 배치가 이미 로드된 경우 예외
 */
class BatchAlreadyLoadedException : public BatchException {
public:
    explicit BatchAlreadyLoadedException(const std::string& batch_id)
        : BatchException("Batch already loaded: " + batch_id) {}
};

/**
 * 배치를 찾을 수 없는 경우 예외
 */
class BatchNotFoundException : public BatchException {
public:
    explicit BatchNotFoundException(const std::string& batch_id)
        : BatchException("Batch not found: " + batch_id) {}
};

/**
 * 손상된 배치 데이터 예외
 */
class CorruptedBatchException : public BatchException {
public:
    explicit CorruptedBatchException(const std::string& batch_id)
        : BatchException("Corrupted batch data: " + batch_id) {}
};

/**
 * 세션 타임아웃 예외
 */
class SessionTimeoutException : public DuraStashException {
public:
    explicit SessionTimeoutException(const std::string& session_id)
        : DuraStashException("Session timeout: " + session_id) {}
};

} // namespace durastash

