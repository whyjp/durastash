#pragma once

#include <string>
#include <cstdint>
#include <chrono>

namespace durastash {

/**
 * ULID (Universally Unique Lexicographically Sortable Identifier) 생성 및 파싱 유틸리티
 * 
 * ULID는 시간순 정렬이 가능한 128비트 식별자입니다.
 * 형식: 01ARZ3NDEKTSV4RRFFQ69G5FAV (26자 Base32 인코딩)
 */
class ULID {
public:
    static constexpr size_t ULID_LENGTH = 26;
    static constexpr size_t TIMESTAMP_LENGTH = 10;
    static constexpr size_t RANDOM_LENGTH = 16;

    /**
     * 새로운 ULID 생성
     * @return ULID 문자열 (26자 Base32)
     */
    static std::string Generate();

    /**
     * 타임스탬프를 포함한 ULID 생성
     * @param timestamp 밀리초 단위 타임스탬프
     * @return ULID 문자열
     */
    static std::string Generate(uint64_t timestamp);

    /**
     * ULID에서 타임스탬프 추출
     * @param ulid ULID 문자열
     * @return 밀리초 단위 타임스탬프 (실패시 0)
     */
    static uint64_t ExtractTimestamp(const std::string& ulid);

    /**
     * ULID 유효성 검증
     * @param ulid 검증할 ULID 문자열
     * @return 유효하면 true
     */
    static bool IsValid(const std::string& ulid);

    /**
     * 현재 시간의 밀리초 타임스탬프 반환
     * @return 밀리초 단위 타임스탬프
     */
    static uint64_t Now();

private:
    static constexpr char ENCODING_CHARS[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    
    static void EncodeTimestamp(uint64_t timestamp, char* buffer);
    static void EncodeRandom(char* buffer);
    static uint64_t DecodeTimestamp(const std::string& ulid);
    static uint64_t DecodeBase32(const std::string& str, size_t start, size_t length);
};

} // namespace durastash

