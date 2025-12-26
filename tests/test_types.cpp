#include <gtest/gtest.h>
#include "durastash/types.h"

using namespace durastash;

TEST(BatchMetadataTest, Serialization) {
    BatchMetadata metadata;
    metadata.SetBatchId("01ARZ3NDEKTSV4RRFFQ69G5FAV");
    metadata.SetSequenceStart(0);
    metadata.SetSequenceEnd(99);
    metadata.SetStatus(BatchStatus::PENDING);
    metadata.SetCreatedAt(1234567890);
    metadata.SetLoadedAt(0);

    // 직렬화
    std::string json = metadata.toJson();
    EXPECT_FALSE(json.empty());

    // 역직렬화
    BatchMetadata loaded;
    loaded.fromJson(json);

    EXPECT_EQ(loaded.GetBatchId(), metadata.GetBatchId());
    EXPECT_EQ(loaded.GetSequenceStart(), metadata.GetSequenceStart());
    EXPECT_EQ(loaded.GetSequenceEnd(), metadata.GetSequenceEnd());
    EXPECT_EQ(loaded.GetStatus(), metadata.GetStatus());
    EXPECT_EQ(loaded.GetCreatedAt(), metadata.GetCreatedAt());
}

TEST(SessionStateTest, Serialization) {
    SessionState state;
    state.SetSessionId("01ARZ3NDEKTSV4RRFFQ69G5FAV");
    state.SetProcessId(12345);
    state.SetStartedAt(1234567890);
    state.SetLastHeartbeat(1234567890);
    state.SetStatus(SessionStatus::ACTIVE);

    // 직렬화
    std::string json = state.toJson();
    EXPECT_FALSE(json.empty());

    // 역직렬화
    SessionState loaded;
    loaded.fromJson(json);

    EXPECT_EQ(loaded.GetSessionId(), state.GetSessionId());
    EXPECT_EQ(loaded.GetProcessId(), state.GetProcessId());
    EXPECT_EQ(loaded.GetStartedAt(), state.GetStartedAt());
    EXPECT_EQ(loaded.GetLastHeartbeat(), state.GetLastHeartbeat());
    EXPECT_EQ(loaded.GetStatus(), state.GetStatus());
}

