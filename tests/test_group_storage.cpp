#include <gtest/gtest.h>
#include "durastash/group_storage.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <ctime>

using namespace durastash;

class GroupStorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 임시 디렉토리 생성
        std::srand(std::time(nullptr));
        std::string test_dir = "test_db_" + std::to_string(std::rand());
        test_db_path_ = std::filesystem::temp_directory_path() / test_dir;
        std::filesystem::remove_all(test_db_path_);
        std::filesystem::create_directories(test_db_path_);
        
        storage_ = std::make_unique<GroupStorage>(test_db_path_.string());
        ASSERT_TRUE(storage_->Initialize());
    }

    void TearDown() override {
        if (storage_) {
            storage_->Shutdown();
        }
        // 테스트 디렉토리 정리
        std::filesystem::remove_all(test_db_path_);
    }

    std::filesystem::path test_db_path_;
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

