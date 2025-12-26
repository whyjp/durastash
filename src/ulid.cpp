#include "durastash/ulid.h"
#include <random>
#include <chrono>
#include <algorithm>
#include <cstring>

namespace durastash {

constexpr char ULID::ENCODING_CHARS[];

std::string ULID::Generate() {
    return Generate(Now());
}

std::string ULID::Generate(uint64_t timestamp) {
    char buffer[ULID_LENGTH + 1];
    
    // 타임스탬프 인코딩 (10자)
    EncodeTimestamp(timestamp, buffer);
    
    // 랜덤 부분 인코딩 (16자)
    EncodeRandom(buffer + TIMESTAMP_LENGTH);
    
    buffer[ULID_LENGTH] = '\0';
    return std::string(buffer);
}

uint64_t ULID::ExtractTimestamp(const std::string& ulid) {
    if (!IsValid(ulid)) {
        return 0;
    }
    return DecodeTimestamp(ulid);
}

bool ULID::IsValid(const std::string& ulid) {
    if (ulid.length() != ULID_LENGTH) {
        return false;
    }
    
    // Base32 문자 검증
    for (char c : ulid) {
        bool valid = false;
        for (size_t i = 0; i < 32; ++i) {
            if (ENCODING_CHARS[i] == c) {
                valid = true;
                break;
            }
        }
        if (!valid) {
            return false;
        }
    }
    
    return true;
}

uint64_t ULID::Now() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

void ULID::EncodeTimestamp(uint64_t timestamp, char* buffer) {
    // 48비트 타임스탬프를 10자 Base32로 인코딩
    for (int i = TIMESTAMP_LENGTH - 1; i >= 0; --i) {
        buffer[i] = ENCODING_CHARS[timestamp & 0x1F];
        timestamp >>= 5;
    }
}

void ULID::EncodeRandom(char* buffer) {
    // 랜덤 생성기 (thread-safe)
    thread_local std::mt19937 generator(
        std::chrono::steady_clock::now().time_since_epoch().count()
    );
    thread_local std::uniform_int_distribution<uint32_t> distribution(0, 31);
    
    // 80비트 랜덤 데이터를 16자 Base32로 인코딩
    uint64_t random_part1 = 0;
    uint64_t random_part2 = 0;
    
    // 랜덤 값 생성
    for (int i = 0; i < 10; ++i) {
        random_part1 |= (static_cast<uint64_t>(distribution(generator)) << (i * 5));
    }
    for (int i = 0; i < 6; ++i) {
        random_part2 |= (static_cast<uint64_t>(distribution(generator)) << (i * 5));
    }
    
    // 인코딩
    for (int i = 0; i < 10; ++i) {
        buffer[i] = ENCODING_CHARS[(random_part1 >> (i * 5)) & 0x1F];
    }
    for (int i = 0; i < 6; ++i) {
        buffer[i + 10] = ENCODING_CHARS[(random_part2 >> (i * 5)) & 0x1F];
    }
}

uint64_t ULID::DecodeTimestamp(const std::string& ulid) {
    return DecodeBase32(ulid, 0, TIMESTAMP_LENGTH);
}

uint64_t ULID::DecodeBase32(const std::string& str, size_t start, size_t length) {
    uint64_t result = 0;
    
    for (size_t i = 0; i < length; ++i) {
        char c = str[start + i];
        
        // Base32 문자를 숫자로 변환
        uint8_t value = 0;
        for (size_t j = 0; j < 32; ++j) {
            if (ENCODING_CHARS[j] == c) {
                value = static_cast<uint8_t>(j);
                break;
            }
        }
        
        result = (result << 5) | value;
    }
    
    return result;
}

} // namespace durastash

