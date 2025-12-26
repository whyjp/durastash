#include <gtest/gtest.h>
#include "durastash/group_storage.h"
#include "test_utils.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <ctime>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace durastash;
using namespace durastash::test_utils;

// Windows에서 콘솔 출력 인코딩을 UTF-8로 설정
namespace {
    struct ConsoleEncodingSetter {
        ConsoleEncodingSetter() {
#ifdef _WIN32
            // 콘솔 출력 코드 페이지를 UTF-8로 설정
            SetConsoleOutputCP(65001);
            // 콘솔 입력 코드 페이지도 UTF-8로 설정
            SetConsoleCP(65001);
#endif
        }
    };
    // 전역 객체로 생성하여 프로그램 시작 시 자동 실행
    ConsoleEncodingSetter g_console_encoding_setter;
}

class GroupStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 고유한 임시 디렉토리 생성 (테스트 간 격리 보장)
        test_dir_guard_ = std::make_unique<TestDirectoryGuard>("test_db");
        
        storage_ = std::make_unique<GroupStorage>(test_dir_guard_->GetPathString());
        ASSERT_TRUE(storage_->Initialize());
    }

    void TearDown() override {
        // 저장소 종료 (RocksDB 파일 잠금 해제)
        if (storage_) {
            storage_->Shutdown();
            storage_.reset();
        }
        
        // 디렉토리 정리 (RAII로 자동 정리되지만 명시적으로 호출)
        if (test_dir_guard_) {
            test_dir_guard_->Cleanup();
            test_dir_guard_.reset();
        }
    }

    std::unique_ptr<TestDirectoryGuard> test_dir_guard_;
    std::unique_ptr<GroupStorage> storage_;
};

TEST_F(GroupStorageTest, InitializeSession) {
    std::string group_key = "test_group";
    EXPECT_TRUE(storage_->InitializeSession(group_key));
    
    std::string session_id = storage_->GetSessionId(group_key);
    EXPECT_FALSE(session_id.empty());
}

TEST_F(GroupStorageTest, SaveAndLoad) {
    std::string group_key = "test_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    // 데이터 저장
    EXPECT_TRUE(storage_->Save(group_key, "data1"));
    EXPECT_TRUE(storage_->Save(group_key, "data2"));
    EXPECT_TRUE(storage_->Save(group_key, "data3"));
    
    // 배치 로드
    auto batches = storage_->LoadBatch(group_key, 100);
    ASSERT_EQ(batches.size(), 1);
    EXPECT_EQ(batches[0].data.size(), 3);
    EXPECT_EQ(batches[0].data[0], "data1");
    EXPECT_EQ(batches[0].data[1], "data2");
    EXPECT_EQ(batches[0].data[2], "data3");
}

TEST_F(GroupStorageTest, BatchAcknowledge) {
    std::string group_key = "test_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    // 데이터 저장
    storage_->Save(group_key, "data1");
    storage_->Save(group_key, "data2");
    
    // 배치 로드
    auto batches = storage_->LoadBatch(group_key, 100);
    ASSERT_EQ(batches.size(), 1);
    
    std::string batch_id = batches[0].batch_id;
    
    // 배치 ACK
    EXPECT_TRUE(storage_->AcknowledgeBatch(group_key, batch_id));
    
    // 다시 로드하면 비어있어야 함
    auto empty_batches = storage_->LoadBatch(group_key, 100);
    EXPECT_EQ(empty_batches.size(), 0);
}

TEST_F(GroupStorageTest, BatchResave) {
    std::string group_key = "test_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    // 데이터 저장
    storage_->Save(group_key, "data1");
    storage_->Save(group_key, "data2");
    storage_->Save(group_key, "data3");
    
    // 배치 로드
    auto batches = storage_->LoadBatch(group_key, 100);
    ASSERT_EQ(batches.size(), 1);
    
    std::string batch_id = batches[0].batch_id;
    std::vector<std::string> remaining_data = {"data2", "data3"};
    
    // Resave
    EXPECT_TRUE(storage_->ResaveBatch(group_key, batch_id, remaining_data));
    
    // 다시 로드하여 확인
    auto resaved_batches = storage_->LoadBatch(group_key, 100);
    ASSERT_EQ(resaved_batches.size(), 1);
    EXPECT_EQ(resaved_batches[0].data.size(), 2);
    EXPECT_EQ(resaved_batches[0].data[0], "data2");
    EXPECT_EQ(resaved_batches[0].data[1], "data3");
}

TEST_F(GroupStorageTest, FIFOOrder) {
    std::string group_key = "test_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    // 여러 데이터 저장
    for (int i = 0; i < 10; ++i) {
        storage_->Save(group_key, "data" + std::to_string(i));
    }
    
    // 배치 로드
    auto batches = storage_->LoadBatch(group_key, 100);
    ASSERT_EQ(batches.size(), 1);
    
    // FIFO 순서 확인
    for (size_t i = 0; i < batches[0].data.size(); ++i) {
        EXPECT_EQ(batches[0].data[i], "data" + std::to_string(i));
    }
}

TEST_F(GroupStorageTest, MultipleGroups) {
    std::string group1 = "group1";
    std::string group2 = "group2";
    
    ASSERT_TRUE(storage_->InitializeSession(group1));
    ASSERT_TRUE(storage_->InitializeSession(group2));
    
    // 각 그룹에 데이터 저장
    storage_->Save(group1, "group1_data");
    storage_->Save(group2, "group2_data");
    
    // 각 그룹에서 로드
    auto batches1 = storage_->LoadBatch(group1, 100);
    auto batches2 = storage_->LoadBatch(group2, 100);
    
    ASSERT_EQ(batches1.size(), 1);
    ASSERT_EQ(batches2.size(), 1);
    EXPECT_EQ(batches1[0].data[0], "group1_data");
    EXPECT_EQ(batches2[0].data[0], "group2_data");
}

TEST_F(GroupStorageTest, BatchSizeLimit) {
    std::string group_key = "test_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    storage_->SetBatchSize(5);
    
    // 배치 크기보다 많은 데이터 저장
    for (int i = 0; i < 12; ++i) {
        storage_->Save(group_key, "data" + std::to_string(i));
    }
    
    // 배치 로드 (배치 크기 제한)
    auto batches = storage_->LoadBatch(group_key, 1); // 1개 배치만 요청
    ASSERT_EQ(batches.size(), 1);
    EXPECT_EQ(batches[0].data.size(), 5); // 첫 번째 배치만 로드
}

TEST_F(GroupStorageTest, BasicLoad) {
    std::string group_key = "test_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    // 데이터 저장
    ASSERT_TRUE(storage_->Save(group_key, "data1"));
    ASSERT_TRUE(storage_->Save(group_key, "data2"));
    ASSERT_TRUE(storage_->Save(group_key, "data3"));
    
    // 기본 로드 (상태 변경 없음, 모든 데이터)
    auto values = storage_->Load(group_key);
    ASSERT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], "data1");
    EXPECT_EQ(values[1], "data2");
    EXPECT_EQ(values[2], "data3");
    
    // 다시 로드 가능 (상태 변경 없음)
    auto values_again = storage_->Load(group_key);
    ASSERT_EQ(values_again.size(), 3);
    EXPECT_EQ(values_again[0], "data1");
}

TEST_F(GroupStorageTest, LoadVsLoadBatch) {
    std::string group_key = "test_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    // 데이터 저장
    storage_->Save(group_key, "data1");
    storage_->Save(group_key, "data2");
    storage_->Save(group_key, "data3");
    
    // 기본 Load (상태 변경 없음)
    auto values = storage_->Load(group_key);
    ASSERT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], "data1");
    
    // LoadBatch (상태 변경 포함)
    auto batches = storage_->LoadBatch(group_key, 100);
    ASSERT_EQ(batches.size(), 1);
    EXPECT_EQ(batches[0].data.size(), 3);
    
    // LoadBatch 후에도 기본 Load는 여전히 가능 (배치 ACK 전까지)
    auto values_after = storage_->Load(group_key);
    ASSERT_EQ(values_after.size(), 3);
    EXPECT_EQ(values_after[0], "data1");
    
    // 배치 ACK 후에는 삭제됨
    storage_->AcknowledgeBatch(group_key, batches[0].batch_id);
    auto values_after_ack = storage_->Load(group_key);
    EXPECT_EQ(values_after_ack.size(), 0); // 삭제됨
}

