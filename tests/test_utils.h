#pragma once

#include <filesystem>
#include <string>
#include <random>
#include <chrono>
#include <thread>

namespace durastash {
namespace test_utils {

/**
 * 테스트용 고유한 임시 디렉토리 생성
 * 각 테스트마다 고유한 디렉토리를 보장하여 테스트 간 격리 보장
 */
inline std::filesystem::path CreateUniqueTestDirectory(const std::string& prefix = "test_db") {
    // 타임스탬프 + 랜덤 숫자로 고유성 보장
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999999);
    
    std::string test_dir = prefix + "_" + 
                          std::to_string(timestamp) + "_" + 
                          std::to_string(dis(gen));
    
    std::filesystem::path test_path = std::filesystem::temp_directory_path() / test_dir;
    
    // 기존 디렉토리가 있으면 삭제 시도
    if (std::filesystem::exists(test_path)) {
        std::filesystem::remove_all(test_path);
    }
    
    // 새 디렉토리 생성
    std::filesystem::create_directories(test_path);
    
    return test_path;
}

/**
 * 테스트 디렉토리 안전하게 삭제
 * RocksDB 파일 잠금 등을 고려하여 재시도 로직 포함
 */
inline bool RemoveTestDirectory(const std::filesystem::path& path, int max_retries = 5) {
    if (!std::filesystem::exists(path)) {
        return true;
    }
    
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        try {
            // 파일 속성 제거 (읽기 전용 등)
            std::filesystem::permissions(path, 
                std::filesystem::perms::all,
                std::filesystem::perm_options::replace);
            
            // 재귀적으로 삭제
            std::filesystem::remove_all(path);
            
            // 삭제 확인
            if (!std::filesystem::exists(path)) {
                return true;
            }
        } catch (const std::filesystem::filesystem_error& e) {
            // 마지막 시도가 아니면 잠시 대기 후 재시도
            if (attempt < max_retries - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100 * (attempt + 1)));
                continue;
            }
            // 마지막 시도 실패 시 경고만 출력 (테스트는 계속 진행)
            std::cerr << "Warning: Failed to remove test directory: " 
                      << path << " - " << e.what() << std::endl;
            return false;
        }
    }
    
    return false;
}

/**
 * 테스트 디렉토리 정리 헬퍼 클래스
 * RAII 패턴으로 테스트 종료 시 자동 정리 보장
 */
class TestDirectoryGuard {
public:
    explicit TestDirectoryGuard(const std::string& prefix = "test_db")
        : path_(CreateUniqueTestDirectory(prefix)) {
    }
    
    ~TestDirectoryGuard() {
        Cleanup();
    }
    
    // 복사 방지
    TestDirectoryGuard(const TestDirectoryGuard&) = delete;
    TestDirectoryGuard& operator=(const TestDirectoryGuard&) = delete;
    
    // 이동 허용
    TestDirectoryGuard(TestDirectoryGuard&& other) noexcept
        : path_(std::move(other.path_)), cleaned_(other.cleaned_) {
        other.cleaned_ = true;
    }
    
    const std::filesystem::path& GetPath() const {
        return path_;
    }
    
    std::string GetPathString() const {
        return path_.string();
    }
    
    void Cleanup() {
        if (!cleaned_ && std::filesystem::exists(path_)) {
            RemoveTestDirectory(path_);
            cleaned_ = true;
        }
    }
    
    // 수동으로 정리하지 않고 유지 (디버깅용)
    void Release() {
        cleaned_ = true;
    }

private:
    std::filesystem::path path_;
    bool cleaned_ = false;
};

} // namespace test_utils
} // namespace durastash

