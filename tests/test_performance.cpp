#include <gtest/gtest.h>
#include "durastash/group_storage.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <random>
#include <algorithm>

using namespace durastash;
using namespace std::chrono;

class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 임시 디렉토리 생성
        std::srand(std::time(nullptr));
        std::string test_dir = "perf_db_" + std::to_string(std::rand());
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

/**
 * 단일 스레드 쓰기 성능 측정
 */
TEST_F(PerformanceTest, SingleThreadWriteThroughput) {
    std::string group_key = "perf_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    const size_t num_operations = 10000;
    const size_t data_size = 1024; // 1KB per operation
    
    std::string data(data_size, 'A');
    
    auto start = high_resolution_clock::now();
    
    for (size_t i = 0; i < num_operations; ++i) {
        ASSERT_TRUE(storage_->Save(group_key, data + std::to_string(i)));
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    double throughput = (double)num_operations * 1000.0 / duration; // ops/sec
    double mbps = (double)num_operations * data_size / (1024.0 * 1024.0) / (duration / 1000.0);
    
    std::cout << "\n=== 단일 스레드 쓰기 성능 ===" << std::endl;
    std::cout << "작업 수: " << num_operations << std::endl;
    std::cout << "데이터 크기: " << data_size << " bytes" << std::endl;
    std::cout << "소요 시간: " << duration << " ms" << std::endl;
    std::cout << "처리량: " << throughput << " ops/sec" << std::endl;
    std::cout << "대역폭: " << mbps << " MB/s" << std::endl;
    
    EXPECT_GT(throughput, 100); // 최소 100 ops/sec 이상
}

/**
 * 단일 스레드 읽기 성능 측정
 */
TEST_F(PerformanceTest, SingleThreadReadThroughput) {
    std::string group_key = "perf_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    const size_t num_operations = 1000;
    const size_t data_size = 1024;
    
    // 데이터 미리 저장
    std::string data(data_size, 'A');
    for (size_t i = 0; i < num_operations; ++i) {
        storage_->Save(group_key, data + std::to_string(i));
    }
    
    auto start = high_resolution_clock::now();
    
    // 모든 데이터 읽기
    auto results = storage_->Load(group_key);
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    double throughput = (double)results.size() * 1000.0 / duration; // ops/sec
    double mbps = (double)results.size() * data_size / (1024.0 * 1024.0) / (duration / 1000.0);
    
    std::cout << "\n=== 단일 스레드 읽기 성능 ===" << std::endl;
    std::cout << "읽은 항목 수: " << results.size() << std::endl;
    std::cout << "데이터 크기: " << data_size << " bytes" << std::endl;
    std::cout << "소요 시간: " << duration << " ms" << std::endl;
    std::cout << "처리량: " << throughput << " ops/sec" << std::endl;
    std::cout << "대역폭: " << mbps << " MB/s" << std::endl;
    
    EXPECT_EQ(results.size(), num_operations);
    EXPECT_GT(throughput, 100); // 최소 100 ops/sec 이상
}

/**
 * 동시 쓰기 성능 측정 (멀티스레드)
 */
TEST_F(PerformanceTest, ConcurrentWriteThroughput) {
    std::string group_key = "perf_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    const size_t num_threads = 4;
    const size_t operations_per_thread = 2500;
    const size_t data_size = 512;
    
    std::atomic<size_t> success_count(0);
    std::atomic<size_t> failure_count(0);
    
    auto worker = [&](size_t thread_id) {
        std::string data(data_size, 'A');
        for (size_t i = 0; i < operations_per_thread; ++i) {
            std::string key = data + "_t" + std::to_string(thread_id) + "_" + std::to_string(i);
            if (storage_->Save(group_key, key)) {
                success_count++;
            } else {
                failure_count++;
            }
        }
    };
    
    auto start = high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    size_t total_operations = success_count + failure_count;
    double throughput = (double)total_operations * 1000.0 / duration; // ops/sec
    double mbps = (double)success_count * data_size / (1024.0 * 1024.0) / (duration / 1000.0);
    
    std::cout << "\n=== 동시 쓰기 성능 (멀티스레드) ===" << std::endl;
    std::cout << "스레드 수: " << num_threads << std::endl;
    std::cout << "스레드당 작업 수: " << operations_per_thread << std::endl;
    std::cout << "성공: " << success_count << ", 실패: " << failure_count << std::endl;
    std::cout << "소요 시간: " << duration << " ms" << std::endl;
    std::cout << "처리량: " << throughput << " ops/sec" << std::endl;
    std::cout << "대역폭: " << mbps << " MB/s" << std::endl;
    
    EXPECT_EQ(failure_count, 0);
    EXPECT_GT(throughput, 100); // 최소 100 ops/sec 이상
}

/**
 * 동시 읽기 성능 측정 (멀티스레드)
 */
TEST_F(PerformanceTest, ConcurrentReadThroughput) {
    std::string group_key = "perf_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    // 데이터 미리 저장
    const size_t num_data = 1000;
    const size_t data_size = 512;
    std::string data(data_size, 'A');
    for (size_t i = 0; i < num_data; ++i) {
        storage_->Save(group_key, data + std::to_string(i));
    }
    
    const size_t num_threads = 4;
    const size_t reads_per_thread = 100;
    
    std::atomic<size_t> total_reads(0);
    
    auto worker = [&]() {
        for (size_t i = 0; i < reads_per_thread; ++i) {
            auto results = storage_->Load(group_key);
            total_reads += results.size();
        }
    };
    
    auto start = high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    double throughput = (double)total_reads * 1000.0 / duration; // ops/sec
    
    std::cout << "\n=== 동시 읽기 성능 (멀티스레드) ===" << std::endl;
    std::cout << "스레드 수: " << num_threads << std::endl;
    std::cout << "스레드당 읽기 횟수: " << reads_per_thread << std::endl;
    std::cout << "총 읽은 항목 수: " << total_reads << std::endl;
    std::cout << "소요 시간: " << duration << " ms" << std::endl;
    std::cout << "처리량: " << throughput << " ops/sec" << std::endl;
    
    EXPECT_GT(throughput, 100); // 최소 100 ops/sec 이상
}

/**
 * 읽기-쓰기 혼합 성능 측정
 */
TEST_F(PerformanceTest, MixedReadWriteThroughput) {
    std::string group_key = "perf_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    const size_t num_threads = 4;
    const size_t operations_per_thread = 1000;
    const size_t data_size = 256;
    
    std::atomic<size_t> write_count(0);
    std::atomic<size_t> read_count(0);
    
    auto writer = [&](size_t thread_id) {
        std::string data(data_size, 'W');
        for (size_t i = 0; i < operations_per_thread; ++i) {
            if (storage_->Save(group_key, data + std::to_string(thread_id) + "_" + std::to_string(i))) {
                write_count++;
            }
        }
    };
    
    auto reader = [&]() {
        for (size_t i = 0; i < operations_per_thread; ++i) {
            auto results = storage_->Load(group_key);
            read_count += results.size();
        }
    };
    
    auto start = high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    // 절반은 쓰기, 절반은 읽기
    for (size_t i = 0; i < num_threads / 2; ++i) {
        threads.emplace_back(writer, i);
    }
    for (size_t i = 0; i < num_threads / 2; ++i) {
        threads.emplace_back(reader);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    double write_throughput = (double)write_count * 1000.0 / duration;
    double read_throughput = (double)read_count * 1000.0 / duration;
    
    std::cout << "\n=== 읽기-쓰기 혼합 성능 ===" << std::endl;
    std::cout << "쓰기 스레드 수: " << num_threads / 2 << std::endl;
    std::cout << "읽기 스레드 수: " << num_threads / 2 << std::endl;
    std::cout << "쓰기 처리량: " << write_throughput << " ops/sec" << std::endl;
    std::cout << "읽기 처리량: " << read_throughput << " ops/sec" << std::endl;
    std::cout << "소요 시간: " << duration << " ms" << std::endl;
    
    EXPECT_GT(write_throughput, 50);
    EXPECT_GT(read_throughput, 50);
}

/**
 * 배치 처리 성능 측정
 */
TEST_F(PerformanceTest, BatchProcessingPerformance) {
    std::string group_key = "perf_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    storage_->SetBatchSize(100);
    
    const size_t num_operations = 1000;
    const size_t data_size = 256;
    
    std::string data(data_size, 'B');
    
    // 데이터 저장
    auto save_start = high_resolution_clock::now();
    for (size_t i = 0; i < num_operations; ++i) {
        storage_->Save(group_key, data + std::to_string(i));
    }
    auto save_end = high_resolution_clock::now();
    auto save_duration = duration_cast<milliseconds>(save_end - save_start).count();
    
    // 배치 로드
    auto load_start = high_resolution_clock::now();
    auto batches = storage_->LoadBatch(group_key, 100);
    auto load_end = high_resolution_clock::now();
    auto load_duration = duration_cast<milliseconds>(load_end - load_start).count();
    
    size_t total_batch_data = 0;
    for (const auto& batch : batches) {
        total_batch_data += batch.data.size();
    }
    
    double save_throughput = (double)num_operations * 1000.0 / save_duration;
    double load_throughput = (double)total_batch_data * 1000.0 / load_duration;
    
    std::cout << "\n=== 배치 처리 성능 ===" << std::endl;
    std::cout << "저장 작업 수: " << num_operations << std::endl;
    std::cout << "배치 수: " << batches.size() << std::endl;
    std::cout << "총 배치 데이터 수: " << total_batch_data << std::endl;
    std::cout << "저장 처리량: " << save_throughput << " ops/sec" << std::endl;
    std::cout << "로드 처리량: " << load_throughput << " ops/sec" << std::endl;
    
    EXPECT_EQ(total_batch_data, num_operations);
}

/**
 * 대용량 데이터 처리 성능 측정
 */
TEST_F(PerformanceTest, LargeDataPerformance) {
    std::string group_key = "perf_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    const size_t num_operations = 100;
    const size_t data_size = 1024 * 1024; // 1MB per operation
    
    std::string data(data_size, 'L');
    
    auto start = high_resolution_clock::now();
    
    for (size_t i = 0; i < num_operations; ++i) {
        ASSERT_TRUE(storage_->Save(group_key, data + std::to_string(i)));
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    double throughput = (double)num_operations * 1000.0 / duration;
    double mbps = (double)num_operations * data_size / (1024.0 * 1024.0) / (duration / 1000.0);
    
    std::cout << "\n=== 대용량 데이터 처리 성능 ===" << std::endl;
    std::cout << "작업 수: " << num_operations << std::endl;
    std::cout << "데이터 크기: " << data_size / (1024 * 1024) << " MB per operation" << std::endl;
    std::cout << "소요 시간: " << duration << " ms" << std::endl;
    std::cout << "처리량: " << throughput << " ops/sec" << std::endl;
    std::cout << "대역폭: " << mbps << " MB/s" << std::endl;
    
    EXPECT_GT(mbps, 1.0); // 최소 1 MB/s 이상
}

/**
 * 다중 그룹 동시 처리 성능 측정
 */
TEST_F(PerformanceTest, MultipleGroupsPerformance) {
    const size_t num_groups = 10;
    const size_t operations_per_group = 100;
    const size_t data_size = 512;
    
    // 모든 그룹 초기화
    for (size_t g = 0; g < num_groups; ++g) {
        std::string group_key = "group_" + std::to_string(g);
        ASSERT_TRUE(storage_->InitializeSession(group_key));
    }
    
    auto start = high_resolution_clock::now();
    
    // 각 그룹에 데이터 저장
    for (size_t g = 0; g < num_groups; ++g) {
        std::string group_key = "group_" + std::to_string(g);
        std::string data(data_size, 'G');
        for (size_t i = 0; i < operations_per_group; ++i) {
            ASSERT_TRUE(storage_->Save(group_key, data + std::to_string(i)));
        }
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    size_t total_operations = num_groups * operations_per_group;
    double throughput = (double)total_operations * 1000.0 / duration;
    
    std::cout << "\n=== 다중 그룹 처리 성능 ===" << std::endl;
    std::cout << "그룹 수: " << num_groups << std::endl;
    std::cout << "그룹당 작업 수: " << operations_per_group << std::endl;
    std::cout << "총 작업 수: " << total_operations << std::endl;
    std::cout << "소요 시간: " << duration << " ms" << std::endl;
    std::cout << "처리량: " << throughput << " ops/sec" << std::endl;
    
    EXPECT_GT(throughput, 100);
}

/**
 * 지연시간 측정 (latency)
 */
TEST_F(PerformanceTest, LatencyMeasurement) {
    std::string group_key = "perf_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    const size_t num_samples = 1000;
    const size_t data_size = 256;
    
    std::vector<double> latencies;
    latencies.reserve(num_samples);
    
    std::string data(data_size, 'L');
    
    for (size_t i = 0; i < num_samples; ++i) {
        auto start = high_resolution_clock::now();
        storage_->Save(group_key, data + std::to_string(i));
        auto end = high_resolution_clock::now();
        
        auto latency_us = duration_cast<microseconds>(end - start).count();
        latencies.push_back(latency_us);
    }
    
    // 통계 계산
    std::sort(latencies.begin(), latencies.end());
    double p50 = latencies[num_samples * 0.5];
    double p95 = latencies[num_samples * 0.95];
    double p99 = latencies[num_samples * 0.99];
    double p999 = latencies[static_cast<size_t>(num_samples * 0.999)];
    double avg = 0;
    for (auto l : latencies) {
        avg += l;
    }
    avg /= num_samples;
    
    std::cout << "\n=== 지연시간 측정 ===" << std::endl;
    std::cout << "샘플 수: " << num_samples << std::endl;
    std::cout << "평균 지연시간: " << avg << " us" << std::endl;
    std::cout << "P50 (중앙값): " << p50 << " us" << std::endl;
    std::cout << "P95: " << p95 << " us" << std::endl;
    std::cout << "P99: " << p99 << " us" << std::endl;
    std::cout << "P99.9: " << p999 << " us" << std::endl;
    
    EXPECT_LT(p95, 10000); // P95가 10ms 이하
}

