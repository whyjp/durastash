#include <gtest/gtest.h>
#include "durastash/ulid.h"
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>

using namespace durastash;

TEST(ULIDTest, Generate) {
    std::string ulid = ULID::Generate();
    EXPECT_EQ(ulid.length(), ULID::ULID_LENGTH);
    EXPECT_TRUE(ULID::IsValid(ulid));
}

TEST(ULIDTest, GenerateWithTimestamp) {
    uint64_t timestamp = ULID::Now();
    std::string ulid = ULID::Generate(timestamp);
    EXPECT_EQ(ulid.length(), ULID::ULID_LENGTH);
    EXPECT_TRUE(ULID::IsValid(ulid));
    
    uint64_t extracted = ULID::ExtractTimestamp(ulid);
    EXPECT_EQ(extracted, timestamp);
}

TEST(ULIDTest, ExtractTimestamp) {
    uint64_t timestamp = 1234567890;
    std::string ulid = ULID::Generate(timestamp);
    uint64_t extracted = ULID::ExtractTimestamp(ulid);
    EXPECT_EQ(extracted, timestamp);
}

TEST(ULIDTest, IsValid) {
    std::string valid_ulid = ULID::Generate();
    EXPECT_TRUE(ULID::IsValid(valid_ulid));
    
    EXPECT_FALSE(ULID::IsValid(""));
    EXPECT_FALSE(ULID::IsValid("invalid"));
    EXPECT_FALSE(ULID::IsValid("01ARZ3NDEKTSV4RRFFQ69G5FA")); // too short
}

TEST(ULIDTest, LexicographicOrder) {
    // 시간순 정렬 테스트
    std::vector<std::string> ulids;
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ulids.push_back(ULID::Generate());
    }
    
    // 정렬 전 복사
    std::vector<std::string> sorted_ulids = ulids;
    std::sort(sorted_ulids.begin(), sorted_ulids.end());
    
    // 원본과 정렬된 버전이 같아야 함 (시간순)
    EXPECT_EQ(ulids, sorted_ulids);
}

