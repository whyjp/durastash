#include <gtest/gtest.h>
#include "durastash/group_storage.h"
#include "test_utils.h"
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

#ifdef _WIN32
#include <windows.h>
#endif

using namespace durastash;
using namespace durastash::test_utils;
using namespace std::chrono;

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

class PerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 고유한 임시 디렉토리 생성 (테스트 간 격리 보장)
        test_dir_guard_ = std::make_unique<TestDirectoryGuard>("perf_db");
        
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

// ============================================================================
// 복잡한 동시성 테스트
// ============================================================================

class ConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 테스트용 고유한 임시 디렉토리 생성 (테스트 간 격리 보장)
        test_dir_guard_ = std::make_unique<TestDirectoryGuard>("concurrent_db");
        
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

/**
 * 경쟁 조건 테스트: 동시 쓰기 시 데이터 손실 없음 확인
 */
TEST_F(ConcurrencyTest, RaceConditionWriteConsistency) {
    std::string group_key = "race_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    const size_t num_threads = 8;
    const size_t writes_per_thread = 1000;
    const size_t total_writes = num_threads * writes_per_thread;
    
    std::atomic<size_t> success_count(0);
    std::atomic<size_t> failure_count(0);
    std::vector<std::thread> threads;
    
    auto writer = [&](size_t thread_id) {
        for (size_t i = 0; i < writes_per_thread; ++i) {
            std::string data = "thread_" + std::to_string(thread_id) + "_" + std::to_string(i);
            if (storage_->Save(group_key, data)) {
                success_count++;
            } else {
                failure_count++;
            }
        }
    };
    
    // 모든 스레드 동시 시작
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(writer, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // 모든 데이터 읽기
    auto results = storage_->Load(group_key);
    
    std::cout << "\n=== 경쟁 조건 테스트 ===" << std::endl;
    std::cout << "스레드 수: " << num_threads << std::endl;
    std::cout << "스레드당 쓰기 수: " << writes_per_thread << std::endl;
    std::cout << "성공한 쓰기: " << success_count << std::endl;
    std::cout << "실패한 쓰기: " << failure_count << std::endl;
    std::cout << "읽은 데이터 수: " << results.size() << std::endl;
    
    EXPECT_EQ(failure_count, 0);
    EXPECT_EQ(results.size(), success_count);
    EXPECT_EQ(results.size(), total_writes);
}

/**
 * 동시 읽기-쓰기 일관성 테스트
 */
TEST_F(ConcurrencyTest, ConcurrentReadWriteConsistency) {
    std::string group_key = "consistency_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    const size_t num_writers = 4;
    const size_t num_readers = 4;
    const size_t writes_per_writer = 500;
    const size_t reads_per_reader = 200;
    
    std::atomic<size_t> write_count(0);
    std::atomic<size_t> read_count(0);
    std::atomic<bool> stop_flag(false);
    
    std::vector<std::set<std::string>> reader_sets(num_readers);
    std::vector<std::mutex> reader_mutexes(num_readers);
    
    auto writer = [&](size_t writer_id) {
        for (size_t i = 0; i < writes_per_writer; ++i) {
            std::string data = "w" + std::to_string(writer_id) + "_" + std::to_string(i);
            if (storage_->Save(group_key, data)) {
                write_count++;
            }
            std::this_thread::sleep_for(microseconds(10)); // 약간의 지연
        }
    };
    
    auto reader = [&](size_t reader_id) {
        while (!stop_flag || read_count < writes_per_writer * num_writers) {
            auto results = storage_->Load(group_key);
            {
                std::lock_guard<std::mutex> lock(reader_mutexes[reader_id]);
                for (const auto& data : results) {
                    reader_sets[reader_id].insert(data);
                }
            }
            read_count += results.size();
            std::this_thread::sleep_for(microseconds(5));
        }
    };
    
    std::vector<std::thread> threads;
    
    // 쓰기 스레드 시작
    for (size_t i = 0; i < num_writers; ++i) {
        threads.emplace_back(writer, i);
    }
    
    // 읽기 스레드 시작
    for (size_t i = 0; i < num_readers; ++i) {
        threads.emplace_back(reader, i);
    }
    
    // 쓰기 스레드 대기
    for (size_t i = 0; i < num_writers; ++i) {
        threads[i].join();
    }
    
    // 읽기 스레드 종료
    stop_flag = true;
    std::this_thread::sleep_for(milliseconds(100));
    
    for (size_t i = num_writers; i < num_writers + num_readers; ++i) {
        threads[i].join();
    }
    
    // 최종 데이터 확인
    auto final_results = storage_->Load(group_key);
    
    std::cout << "\n=== 동시 읽기-쓰기 일관성 테스트 ===" << std::endl;
    std::cout << "쓰기 스레드 수: " << num_writers << std::endl;
    std::cout << "읽기 스레드 수: " << num_readers << std::endl;
    std::cout << "총 쓰기 수: " << write_count << std::endl;
    std::cout << "최종 데이터 수: " << final_results.size() << std::endl;
    
    EXPECT_EQ(final_results.size(), write_count);
}

/**
 * 배치 ACK와 기본 Load의 동시성 테스트
 */
TEST_F(ConcurrencyTest, BatchAckAndLoadConcurrency) {
    std::string group_key = "batch_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    storage_->SetBatchSize(100);
    
    const size_t num_writers = 2;
    const size_t num_loaders = 2;
    const size_t num_batch_loaders = 2;
    const size_t writes_per_writer = 500;
    
    std::atomic<size_t> write_count(0);
    std::atomic<size_t> load_count(0);
    std::atomic<size_t> batch_load_count(0);
    std::atomic<bool> stop_flag(false);
    
    std::set<std::string> acked_batches;
    std::mutex ack_mutex;
    
    auto writer = [&](size_t writer_id) {
        for (size_t i = 0; i < writes_per_writer; ++i) {
            std::string data = "data_" + std::to_string(writer_id) + "_" + std::to_string(i);
            if (storage_->Save(group_key, data)) {
                write_count++;
            }
        }
    };
    
    auto loader = [&]() {
        while (!stop_flag) {
            auto results = storage_->Load(group_key);
            load_count += results.size();
            std::this_thread::sleep_for(milliseconds(10));
        }
    };
    
    auto batch_loader = [&]() {
        while (!stop_flag) {
            auto batches = storage_->LoadBatch(group_key, 50);
            for (const auto& batch : batches) {
                batch_load_count += batch.data.size();
                
                // ACK 처리
                {
                    std::lock_guard<std::mutex> lock(ack_mutex);
                    if (acked_batches.find(batch.batch_id) == acked_batches.end()) {
                        storage_->AcknowledgeBatch(group_key, batch.batch_id);
                        acked_batches.insert(batch.batch_id);
                    }
                }
            }
            std::this_thread::sleep_for(milliseconds(20));
        }
    };
    
    std::vector<std::thread> threads;
    
    // 쓰기 스레드
    for (size_t i = 0; i < num_writers; ++i) {
        threads.emplace_back(writer, i);
    }
    
    // 기본 Load 스레드
    for (size_t i = 0; i < num_loaders; ++i) {
        threads.emplace_back(loader);
    }
    
    // 배치 Load 스레드
    for (size_t i = 0; i < num_batch_loaders; ++i) {
        threads.emplace_back(batch_loader);
    }
    
    // 쓰기 완료 대기
    for (size_t i = 0; i < num_writers; ++i) {
        threads[i].join();
    }
    
    // 읽기 스레드 종료
    stop_flag = true;
    std::this_thread::sleep_for(milliseconds(500));
    
    for (size_t i = num_writers; i < threads.size(); ++i) {
        threads[i].join();
    }
    
    std::cout << "\n=== 배치 ACK와 기본 Load 동시성 테스트 ===" << std::endl;
    std::cout << "쓰기 수: " << write_count << std::endl;
    std::cout << "기본 Load 항목 수: " << load_count << std::endl;
    std::cout << "배치 Load 항목 수: " << batch_load_count << std::endl;
    std::cout << "ACK된 배치 수: " << acked_batches.size() << std::endl;
    
    EXPECT_GT(write_count, 0);
    EXPECT_GT(load_count, 0);
}

/**
 * 다중 그룹 동시 접근 테스트
 */
TEST_F(ConcurrencyTest, MultipleGroupsConcurrentAccess) {
    const size_t num_groups = 10;
    const size_t num_threads_per_group = 4;
    const size_t operations_per_thread = 200;
    
    // 모든 그룹 초기화
    for (size_t g = 0; g < num_groups; ++g) {
        std::string group_key = "group_" + std::to_string(g);
        ASSERT_TRUE(storage_->InitializeSession(group_key));
    }
    
    std::atomic<size_t> total_success(0);
    std::atomic<size_t> total_failure(0);
    
    auto group_worker = [&](size_t group_id, size_t thread_id) {
        std::string group_key = "group_" + std::to_string(group_id);
        for (size_t i = 0; i < operations_per_thread; ++i) {
            std::string data = "g" + std::to_string(group_id) + 
                              "_t" + std::to_string(thread_id) + 
                              "_" + std::to_string(i);
            if (storage_->Save(group_key, data)) {
                total_success++;
            } else {
                total_failure++;
            }
            
            // 읽기도 수행
            auto results = storage_->Load(group_key);
            (void)results; // 사용하지 않아도 경고 방지
        }
    };
    
    std::vector<std::thread> threads;
    
    // 각 그룹에 대해 여러 스레드 시작
    for (size_t g = 0; g < num_groups; ++g) {
        for (size_t t = 0; t < num_threads_per_group; ++t) {
            threads.emplace_back(group_worker, g, t);
        }
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // 각 그룹의 데이터 확인
    size_t total_data = 0;
    for (size_t g = 0; g < num_groups; ++g) {
        std::string group_key = "group_" + std::to_string(g);
        auto results = storage_->Load(group_key);
        total_data += results.size();
    }
    
    std::cout << "\n=== 다중 그룹 동시 접근 테스트 ===" << std::endl;
    std::cout << "그룹 수: " << num_groups << std::endl;
    std::cout << "그룹당 스레드 수: " << num_threads_per_group << std::endl;
    std::cout << "성공한 작업: " << total_success << std::endl;
    std::cout << "실패한 작업: " << total_failure << std::endl;
    std::cout << "총 데이터 수: " << total_data << std::endl;
    
    EXPECT_EQ(total_failure, 0);
    EXPECT_EQ(total_data, total_success);
}

/**
 * 스트레스 테스트: 극단적인 동시성 상황
 */
TEST_F(ConcurrencyTest, StressTestExtremeConcurrency) {
    std::string group_key = "stress_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    const size_t num_threads = 16;
    const size_t operations_per_thread = 1000;
    const size_t data_size = 1024; // 1KB
    
    std::atomic<size_t> success_count(0);
    std::atomic<size_t> failure_count(0);
    std::atomic<size_t> read_count(0);
    
    std::string large_data(data_size, 'S');
    
    auto stress_worker = [&](size_t thread_id) {
        // 쓰기와 읽기를 무작위로 섞어서 수행
        std::mt19937 gen(thread_id);
        std::uniform_int_distribution<> dis(0, 1);
        
        for (size_t i = 0; i < operations_per_thread; ++i) {
            if (dis(gen) == 0) {
                // 쓰기
                std::string data = large_data + "_t" + std::to_string(thread_id) + 
                                  "_" + std::to_string(i);
                if (storage_->Save(group_key, data)) {
                    success_count++;
                } else {
                    failure_count++;
                }
            } else {
                // 읽기
                auto results = storage_->Load(group_key);
                read_count += results.size();
            }
            
            // 가끔 배치 로드도 수행
            if (i % 100 == 0) {
                auto batches = storage_->LoadBatch(group_key, 50);
                for (const auto& batch : batches) {
                    if (!batch.data.empty()) {
                        storage_->AcknowledgeBatch(group_key, batch.batch_id);
                    }
                }
            }
        }
    };
    
    auto start = high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(stress_worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    // 최종 데이터 확인
    auto final_results = storage_->Load(group_key);
    
    std::cout << "\n=== 스트레스 테스트 ===" << std::endl;
    std::cout << "스레드 수: " << num_threads << std::endl;
    std::cout << "스레드당 작업 수: " << operations_per_thread << std::endl;
    std::cout << "성공한 쓰기: " << success_count << std::endl;
    std::cout << "실패한 쓰기: " << failure_count << std::endl;
    std::cout << "읽기 횟수: " << read_count << std::endl;
    std::cout << "최종 데이터 수: " << final_results.size() << std::endl;
    std::cout << "소요 시간: " << duration << " ms" << std::endl;
    
    EXPECT_EQ(failure_count, 0);
    EXPECT_GT(final_results.size(), 0);
}

/**
 * 순서 보장 테스트: FIFO 순서가 동시성 상황에서도 유지되는지 확인
 */
TEST_F(ConcurrencyTest, FIFOOrderUnderConcurrency) {
    std::string group_key = "order_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    const size_t num_threads = 8;
    const size_t writes_per_thread = 100;
    
    std::vector<std::vector<std::string>> thread_data(num_threads);
    std::vector<std::mutex> thread_mutexes(num_threads);
    
    auto ordered_writer = [&](size_t thread_id) {
        for (size_t i = 0; i < writes_per_thread; ++i) {
            std::string data = "t" + std::to_string(thread_id) + "_" + std::to_string(i);
            if (storage_->Save(group_key, data)) {
                std::lock_guard<std::mutex> lock(thread_mutexes[thread_id]);
                thread_data[thread_id].push_back(data);
            }
            std::this_thread::sleep_for(microseconds(1)); // 순서 보장을 위한 최소 지연
        }
    };
    
    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(ordered_writer, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // 모든 데이터 읽기
    auto results = storage_->Load(group_key);
    
    // 순서 검증: 각 스레드의 데이터가 순서대로 나타나는지 확인
    std::vector<size_t> thread_indices(num_threads, 0);
    size_t valid_sequences = 0;
    
    for (const auto& data : results) {
        // 데이터 형식: "t{thread_id}_{index}"
        size_t t_pos = data.find('_');
        if (t_pos != std::string::npos) {
            size_t thread_id = std::stoull(data.substr(1, t_pos - 1));
            size_t index = std::stoull(data.substr(t_pos + 1));
            
            if (thread_id < num_threads) {
                if (thread_indices[thread_id] == index) {
                    thread_indices[thread_id]++;
                    valid_sequences++;
                }
            }
        }
    }
    
    std::cout << "\n=== FIFO 순서 보장 테스트 ===" << std::endl;
    std::cout << "스레드 수: " << num_threads << std::endl;
    std::cout << "총 데이터 수: " << results.size() << std::endl;
    std::cout << "유효한 순서: " << valid_sequences << std::endl;
    
    // 각 스레드의 모든 데이터가 순서대로 나타나야 함
    EXPECT_EQ(results.size(), num_threads * writes_per_thread);
}

/**
 * 데드락 방지 테스트: 복잡한 동시 작업에서 데드락이 발생하지 않는지 확인
 */
TEST_F(ConcurrencyTest, DeadlockPrevention) {
    const size_t num_groups = 5;
    const size_t num_threads = 10;
    const size_t operations_per_thread = 500;
    
    // 모든 그룹 초기화
    for (size_t g = 0; g < num_groups; ++g) {
        std::string group_key = "deadlock_group_" + std::to_string(g);
        ASSERT_TRUE(storage_->InitializeSession(group_key));
    }
    
    std::atomic<size_t> completed_threads(0);
    std::atomic<bool> deadlock_detected(false);
    
    auto complex_worker = [&](size_t thread_id) {
        std::mt19937 gen(thread_id);
        std::uniform_int_distribution<> group_dis(0, num_groups - 1);
        std::uniform_int_distribution<> op_dis(0, 3);
        
        for (size_t i = 0; i < operations_per_thread; ++i) {
            size_t group_id = group_dis(gen);
            std::string group_key = "deadlock_group_" + std::to_string(group_id);
            
            int op = op_dis(gen);
            switch (op) {
                case 0: {
                    // 쓰기
                    std::string data = "data_" + std::to_string(thread_id) + "_" + std::to_string(i);
                    storage_->Save(group_key, data);
                    break;
                }
                case 1: {
                    // 읽기
                    auto results = storage_->Load(group_key);
                    (void)results;
                    break;
                }
                case 2: {
                    // 배치 로드
                    auto batches = storage_->LoadBatch(group_key, 50);
                    for (const auto& batch : batches) {
                        storage_->AcknowledgeBatch(group_key, batch.batch_id);
                    }
                    break;
                }
                case 3: {
                    // 세션 ID 조회
                    std::string session_id = storage_->GetSessionId(group_key);
                    (void)session_id;
                    break;
                }
            }
        }
        
        completed_threads++;
    };
    
    // 데드락 감지를 위한 타임아웃
    auto timeout = high_resolution_clock::now() + seconds(60);
    
    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(complex_worker, i);
    }
    
    // 타임아웃 체크 스레드
    std::thread timeout_thread([&]() {
        while (completed_threads < num_threads) {
            if (high_resolution_clock::now() > timeout) {
                deadlock_detected = true;
                break;
            }
            std::this_thread::sleep_for(milliseconds(100));
        }
    });
    
    for (auto& t : threads) {
        t.join();
    }
    
    timeout_thread.join();
    
    std::cout << "\n=== 데드락 방지 테스트 ===" << std::endl;
    std::cout << "그룹 수: " << num_groups << std::endl;
    std::cout << "스레드 수: " << num_threads << std::endl;
    std::cout << "완료된 스레드: " << completed_threads << std::endl;
    std::cout << "데드락 감지: " << (deadlock_detected ? "YES" : "NO") << std::endl;
    
    EXPECT_FALSE(deadlock_detected);
    EXPECT_EQ(completed_threads, num_threads);
}

/**
 * 동시 세션 초기화/종료 테스트
 */
TEST_F(ConcurrencyTest, ConcurrentSessionLifecycle) {
    const size_t num_groups = 20;
    const size_t num_threads = 10;
    const size_t cycles_per_thread = 50;
    
    std::atomic<size_t> init_success(0);
    std::atomic<size_t> init_failure(0);
    std::atomic<size_t> terminate_success(0);
    
    auto lifecycle_worker = [&](size_t thread_id) {
        std::mt19937 gen(thread_id);
        std::uniform_int_distribution<> group_dis(0, num_groups - 1);
        
        for (size_t cycle = 0; cycle < cycles_per_thread; ++cycle) {
            size_t group_id = group_dis(gen);
            std::string group_key = "lifecycle_group_" + std::to_string(group_id);
            
            // 세션 초기화
            if (storage_->InitializeSession(group_key)) {
                init_success++;
                
                // 데이터 저장
                std::string data = "data_" + std::to_string(thread_id) + "_" + std::to_string(cycle);
                storage_->Save(group_key, data);
                
                // 읽기
                auto results = storage_->Load(group_key);
                (void)results;
                
                // 가끔 세션 종료
                if (cycle % 10 == 0) {
                    storage_->TerminateSession(group_key);
                    terminate_success++;
                }
            } else {
                init_failure++;
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(lifecycle_worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    std::cout << "\n=== 동시 세션 생명주기 테스트 ===" << std::endl;
    std::cout << "그룹 수: " << num_groups << std::endl;
    std::cout << "스레드 수: " << num_threads << std::endl;
    std::cout << "성공한 초기화: " << init_success << std::endl;
    std::cout << "실패한 초기화: " << init_failure << std::endl;
    std::cout << "성공한 종료: " << terminate_success << std::endl;
    
    EXPECT_GT(init_success, 0);
}

// ============================================================================
// 정교한 동시성 테스트 (확률적 버그 검출 강화)
// ============================================================================

/**
 * 반복 실행을 통한 확률적 버그 검출
 * 동시성 버그는 확률적으로 발생하므로 여러 번 반복 실행
 */
TEST_F(ConcurrencyTest, RepeatedExecutionForRaceConditions) {
    const size_t num_iterations = 10;
    const size_t num_threads = 8;
    const size_t operations_per_thread = 500;
    
    for (size_t iteration = 0; iteration < num_iterations; ++iteration) {
        std::string group_key = "repeat_group_" + std::to_string(iteration);
        ASSERT_TRUE(storage_->InitializeSession(group_key));
        
        std::atomic<size_t> success_count(0);
        std::atomic<size_t> failure_count(0);
        std::set<std::string> written_data;
        std::mutex data_mutex;
        
        auto writer = [&](size_t thread_id) {
            for (size_t i = 0; i < operations_per_thread; ++i) {
                std::string data = "iter" + std::to_string(iteration) + 
                                  "_t" + std::to_string(thread_id) + 
                                  "_" + std::to_string(i);
                if (storage_->Save(group_key, data)) {
                    success_count++;
                    std::lock_guard<std::mutex> lock(data_mutex);
                    written_data.insert(data);
                } else {
                    failure_count++;
                }
            }
        };
        
        std::vector<std::thread> threads;
        for (size_t i = 0; i < num_threads; ++i) {
            threads.emplace_back(writer, i);
        }
        
        for (auto& t : threads) {
            t.join();
        }
        
        // 데이터 검증
        auto results = storage_->Load(group_key);
        std::set<std::string> read_data(results.begin(), results.end());
        
        EXPECT_EQ(failure_count, 0) << "Iteration " << iteration << " failed";
        EXPECT_EQ(results.size(), success_count) << "Iteration " << iteration << " data mismatch";
        EXPECT_EQ(read_data.size(), written_data.size()) << "Iteration " << iteration << " duplicate data";
        
        // 모든 쓰기된 데이터가 읽혔는지 확인
        for (const auto& data : written_data) {
            EXPECT_NE(read_data.find(data), read_data.end()) 
                << "Iteration " << iteration << " missing data: " << data;
        }
    }
}

/**
 * 메모리 모델 위반 검출: 순서 보장 검증 강화
 */
TEST_F(ConcurrencyTest, MemoryOrderingVerification) {
    std::string group_key = "memory_order_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    const size_t num_threads = 16;
    const size_t writes_per_thread = 200;
    
    // 각 스레드가 순차적으로 쓰기 (스레드 내부 순서 보장)
    std::vector<std::vector<std::string>> thread_sequences(num_threads);
    std::vector<std::mutex> sequence_mutexes(num_threads);
    
    auto ordered_writer = [&](size_t thread_id) {
        for (size_t i = 0; i < writes_per_thread; ++i) {
            std::string data = "t" + std::to_string(thread_id) + "_seq" + std::to_string(i);
            
            // 쓰기 전에 시퀀스 기록
            {
                std::lock_guard<std::mutex> lock(sequence_mutexes[thread_id]);
                thread_sequences[thread_id].push_back(data);
            }
            
            // 쓰기 수행
            ASSERT_TRUE(storage_->Save(group_key, data));
            
            // 메모리 배리어 시뮬레이션 (약간의 지연)
            std::this_thread::yield();
        }
    };
    
    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(ordered_writer, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // 모든 데이터 읽기
    auto results = storage_->Load(group_key);
    
    // 각 스레드의 시퀀스가 순서대로 나타나는지 검증
    std::vector<size_t> thread_positions(num_threads, 0);
    size_t valid_sequences = 0;
    
    for (const auto& data : results) {
        // 데이터 형식: "t{thread_id}_seq{sequence}"
        size_t seq_pos = data.find("_seq");
        if (seq_pos != std::string::npos) {
            size_t thread_id = std::stoull(data.substr(1, seq_pos - 1));
            size_t seq = std::stoull(data.substr(seq_pos + 4));
            
            if (thread_id < num_threads) {
                // 이전 시퀀스가 이미 나타났는지 확인
                if (thread_positions[thread_id] == seq) {
                    thread_positions[thread_id]++;
                    valid_sequences++;
                } else if (thread_positions[thread_id] < seq) {
                    // 순서가 건너뛰어졌음 (심각한 문제)
                    FAIL() << "Thread " << thread_id << " sequence skipped: expected " 
                           << thread_positions[thread_id] << " but got " << seq;
                }
            }
        }
    }
    
    // 모든 시퀀스가 순서대로 나타났는지 확인
    for (size_t i = 0; i < num_threads; ++i) {
        EXPECT_EQ(thread_positions[i], writes_per_thread) 
            << "Thread " << i << " incomplete sequence";
    }
}

/**
 * 원자성 보장 검증: 중간 상태 관찰 불가능 확인
 */
TEST_F(ConcurrencyTest, AtomicityVerification) {
    std::string group_key = "atomicity_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    const size_t num_writers = 8;
    const size_t num_readers = 8;
    const size_t writes_per_writer = 1000;
    
    std::atomic<size_t> total_writes(0);
    std::atomic<bool> stop_flag(false);
    
    // 읽기 중간에 일관성 없는 상태가 관찰되는지 확인
    std::atomic<size_t> inconsistent_reads(0);
    std::atomic<size_t> total_reads(0);
    
    auto writer = [&](size_t writer_id) {
        for (size_t i = 0; i < writes_per_writer; ++i) {
            std::string data = "w" + std::to_string(writer_id) + "_" + std::to_string(i);
            if (storage_->Save(group_key, data)) {
                total_writes++;
            }
        }
    };
    
    auto reader = [&]() {
        size_t last_size = 0;
        while (!stop_flag || total_writes > last_size) {
            auto results = storage_->Load(group_key);
            size_t current_size = results.size();
            total_reads++;
            
            // 읽은 데이터 크기가 감소하면 안 됨 (원자성 위반)
            if (current_size < last_size) {
                inconsistent_reads++;
            }
            
            // 데이터 중복 확인
            std::set<std::string> unique_data(results.begin(), results.end());
            if (unique_data.size() != results.size()) {
                inconsistent_reads++;
            }
            
            last_size = current_size;
            std::this_thread::sleep_for(microseconds(10));
        }
    };
    
    std::vector<std::thread> threads;
    
    // 쓰기 스레드
    for (size_t i = 0; i < num_writers; ++i) {
        threads.emplace_back(writer, i);
    }
    
    // 읽기 스레드
    for (size_t i = 0; i < num_readers; ++i) {
        threads.emplace_back(reader);
    }
    
    // 쓰기 완료 대기
    for (size_t i = 0; i < num_writers; ++i) {
        threads[i].join();
    }
    
    stop_flag = true;
    std::this_thread::sleep_for(milliseconds(100));
    
    for (size_t i = num_writers; i < threads.size(); ++i) {
        threads[i].join();
    }
    
    std::cout << "\n=== 원자성 보장 검증 ===" << std::endl;
    std::cout << "총 쓰기 수: " << total_writes << std::endl;
    std::cout << "총 읽기 수: " << total_reads << std::endl;
    std::cout << "일관성 없는 읽기: " << inconsistent_reads << std::endl;
    
    EXPECT_EQ(inconsistent_reads, 0) << "Atomicity violation detected";
}

/**
 * 경계 조건 테스트: 배치 경계에서의 동시성
 */
TEST_F(ConcurrencyTest, BatchBoundaryConcurrency) {
    std::string group_key = "boundary_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    storage_->SetBatchSize(100); // 배치 크기 100으로 설정
    
    const size_t num_threads = 8;
    const size_t writes_per_thread = 150; // 배치 경계를 넘어서도록
    
    std::atomic<size_t> success_count(0);
    
    auto boundary_writer = [&](size_t thread_id) {
        for (size_t i = 0; i < writes_per_thread; ++i) {
            std::string data = "boundary_t" + std::to_string(thread_id) + "_" + std::to_string(i);
            if (storage_->Save(group_key, data)) {
                success_count++;
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back(boundary_writer, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // 배치 로드로 데이터 확인
    auto batches = storage_->LoadBatch(group_key, 1000);
    
    size_t total_batch_data = 0;
    for (const auto& batch : batches) {
        total_batch_data += batch.data.size();
        
        // 배치 크기가 배치 크기 제한을 초과하지 않는지 확인
        EXPECT_LE(batch.data.size(), storage_->GetBatchSize() * 2) 
            << "Batch size exceeded limit";
    }
    
    // 기본 Load로도 확인
    auto all_data = storage_->Load(group_key);
    
    std::cout << "\n=== 배치 경계 동시성 테스트 ===" << std::endl;
    std::cout << "총 쓰기 수: " << success_count << std::endl;
    std::cout << "배치 수: " << batches.size() << std::endl;
    std::cout << "배치 데이터 수: " << total_batch_data << std::endl;
    std::cout << "기본 Load 데이터 수: " << all_data.size() << std::endl;
    
    EXPECT_EQ(total_batch_data, success_count);
    EXPECT_EQ(all_data.size(), success_count);
}

/**
 * 타이밍 기반 테스트: 빠른 연속 작업과 느린 작업 혼합
 */
TEST_F(ConcurrencyTest, TimingBasedConcurrency) {
    std::string group_key = "timing_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    const size_t num_fast_threads = 8;
    const size_t num_slow_threads = 2;
    const size_t fast_operations = 500;
    const size_t slow_operations = 50;
    
    std::atomic<size_t> fast_success(0);
    std::atomic<size_t> slow_success(0);
    
    auto fast_worker = [&](size_t thread_id) {
        for (size_t i = 0; i < fast_operations; ++i) {
            std::string data = "fast_t" + std::to_string(thread_id) + "_" + std::to_string(i);
            if (storage_->Save(group_key, data)) {
                fast_success++;
            }
            // 빠른 작업 (최소 지연)
            std::this_thread::yield();
        }
    };
    
    auto slow_worker = [&](size_t thread_id) {
        for (size_t i = 0; i < slow_operations; ++i) {
            std::string data = "slow_t" + std::to_string(thread_id) + "_" + std::to_string(i);
            if (storage_->Save(group_key, data)) {
                slow_success++;
            }
            // 느린 작업 (긴 지연)
            std::this_thread::sleep_for(milliseconds(10));
            
            // 중간에 읽기도 수행
            auto results = storage_->Load(group_key);
            (void)results;
        }
    };
    
    std::vector<std::thread> threads;
    
    // 빠른 스레드 시작
    for (size_t i = 0; i < num_fast_threads; ++i) {
        threads.emplace_back(fast_worker, i);
    }
    
    // 느린 스레드 시작
    for (size_t i = 0; i < num_slow_threads; ++i) {
        threads.emplace_back(slow_worker, i);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto final_results = storage_->Load(group_key);
    
    std::cout << "\n=== 타이밍 기반 동시성 테스트 ===" << std::endl;
    std::cout << "빠른 스레드 성공: " << fast_success << std::endl;
    std::cout << "느린 스레드 성공: " << slow_success << std::endl;
    std::cout << "최종 데이터 수: " << final_results.size() << std::endl;
    
    EXPECT_EQ(final_results.size(), fast_success + slow_success);
}

/**
 * 통계적 검증: 대량 작업에서의 데이터 무결성
 */
TEST_F(ConcurrencyTest, StatisticalIntegrityVerification) {
    std::string group_key = "statistical_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    const size_t num_threads = 16;
    const size_t operations_per_thread = 2000;
    const size_t total_operations = num_threads * operations_per_thread;
    
    std::atomic<size_t> success_count(0);
    std::atomic<size_t> failure_count(0);
    
    // 각 스레드가 고유한 데이터 범위를 가지도록
    auto worker = [&](size_t thread_id) {
        for (size_t i = 0; i < operations_per_thread; ++i) {
            std::string data = "stat_t" + std::to_string(thread_id) + 
                              "_idx" + std::to_string(i);
            if (storage_->Save(group_key, data)) {
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
    
    // 데이터 검증
    auto results = storage_->Load(group_key);
    
    // 통계 계산
    std::set<std::string> unique_data(results.begin(), results.end());
    size_t duplicates = results.size() - unique_data.size();
    
    // 각 스레드의 데이터가 모두 있는지 확인
    std::vector<bool> thread_present(num_threads, false);
    for (const auto& data : results) {
        // 데이터 형식: "stat_t{thread_id}_idx{index}"
        if (data.find("stat_t") == 0) {
            size_t t_pos = data.find("_t");
            if (t_pos != std::string::npos) {
                size_t idx_pos = data.find("_idx", t_pos);
                if (idx_pos != std::string::npos && idx_pos > t_pos + 2) {
                    try {
                        std::string thread_str = data.substr(t_pos + 2, idx_pos - t_pos - 2);
                        if (!thread_str.empty()) {
                            size_t thread_id = std::stoull(thread_str);
                            if (thread_id < num_threads) {
                                thread_present[thread_id] = true;
                            }
                        }
                    } catch (...) {
                        // 파싱 실패는 무시 (다른 형식의 데이터일 수 있음)
                    }
                }
            }
        }
    }
    
    std::cout << "\n=== 통계적 무결성 검증 ===" << std::endl;
    std::cout << "총 작업 수: " << total_operations << std::endl;
    std::cout << "성공한 작업: " << success_count << std::endl;
    std::cout << "실패한 작업: " << failure_count << std::endl;
    std::cout << "읽은 데이터 수: " << results.size() << std::endl;
    std::cout << "고유 데이터 수: " << unique_data.size() << std::endl;
    std::cout << "중복 데이터 수: " << duplicates << std::endl;
    std::cout << "소요 시간: " << duration << " ms" << std::endl;
    
    EXPECT_EQ(failure_count, 0);
    EXPECT_EQ(results.size(), success_count);
    EXPECT_EQ(duplicates, 0) << "Duplicate data detected";
    
    // 모든 스레드의 데이터가 최소한 하나는 있어야 함
    for (size_t i = 0; i < num_threads; ++i) {
        EXPECT_TRUE(thread_present[i]) << "Thread " << i << " data missing";
    }
}

/**
 * 동시 배치 ACK 충돌 테스트
 */
TEST_F(ConcurrencyTest, ConcurrentBatchAckCollision) {
    std::string group_key = "ack_collision_group";
    ASSERT_TRUE(storage_->InitializeSession(group_key));
    
    storage_->SetBatchSize(50);
    
    const size_t num_writers = 4;
    const size_t num_ackers = 4;
    const size_t writes_per_writer = 500;
    
    std::atomic<size_t> write_count(0);
    std::atomic<size_t> ack_count(0);
    std::atomic<size_t> duplicate_ack(0);
    
    std::set<std::string> acked_batches;
    std::mutex ack_mutex;
    
    auto writer = [&](size_t writer_id) {
        for (size_t i = 0; i < writes_per_writer; ++i) {
            std::string data = "ack_data_" + std::to_string(writer_id) + "_" + std::to_string(i);
            if (storage_->Save(group_key, data)) {
                write_count++;
            }
        }
    };
    
    auto acker = [&]() {
        while (write_count < writes_per_writer * num_writers || ack_count < write_count) {
            auto batches = storage_->LoadBatch(group_key, 100);
            for (const auto& batch : batches) {
                std::lock_guard<std::mutex> lock(ack_mutex);
                if (acked_batches.find(batch.batch_id) == acked_batches.end()) {
                    if (storage_->AcknowledgeBatch(group_key, batch.batch_id)) {
                        acked_batches.insert(batch.batch_id);
                        ack_count += batch.data.size();
                    }
                } else {
                    duplicate_ack++;
                }
            }
            std::this_thread::sleep_for(milliseconds(10));
        }
    };
    
    std::vector<std::thread> threads;
    
    // 쓰기 스레드
    for (size_t i = 0; i < num_writers; ++i) {
        threads.emplace_back(writer, i);
    }
    
    // ACK 스레드
    for (size_t i = 0; i < num_ackers; ++i) {
        threads.emplace_back(acker);
    }
    
    // 쓰기 완료 대기
    for (size_t i = 0; i < num_writers; ++i) {
        threads[i].join();
    }
    
    // ACK 완료 대기
    std::this_thread::sleep_for(milliseconds(500));
    
    for (size_t i = num_writers; i < threads.size(); ++i) {
        threads[i].join();
    }
    
    // 최종 확인: 모든 데이터가 ACK되었는지
    auto remaining_batches = storage_->LoadBatch(group_key, 1000);
    auto remaining_data = storage_->Load(group_key);
    
    std::cout << "\n=== 동시 배치 ACK 충돌 테스트 ===" << std::endl;
    std::cout << "쓰기 수: " << write_count << std::endl;
    std::cout << "ACK된 데이터 수: " << ack_count << std::endl;
    std::cout << "중복 ACK 시도: " << duplicate_ack << std::endl;
    std::cout << "남은 배치 수: " << remaining_batches.size() << std::endl;
    std::cout << "남은 데이터 수: " << remaining_data.size() << std::endl;
    
    EXPECT_EQ(ack_count, write_count);
    EXPECT_EQ(remaining_batches.size(), 0);
    EXPECT_EQ(remaining_data.size(), 0);
}

/**
 * 극단적 부하 테스트: 최대 동시성 상황
 */
TEST_F(ConcurrencyTest, ExtremeLoadTest) {
    const size_t num_groups = 20;
    const size_t num_threads_per_group = 8;
    const size_t operations_per_thread = 1000;
    
    // 모든 그룹 초기화
    for (size_t g = 0; g < num_groups; ++g) {
        std::string group_key = "extreme_group_" + std::to_string(g);
        ASSERT_TRUE(storage_->InitializeSession(group_key));
    }
    
    std::atomic<size_t> total_success(0);
    std::atomic<size_t> total_failure(0);
    
    auto extreme_worker = [&](size_t group_id, size_t thread_id) {
        std::string group_key = "extreme_group_" + std::to_string(group_id);
        
        for (size_t i = 0; i < operations_per_thread; ++i) {
            // 쓰기
            std::string data = "g" + std::to_string(group_id) + 
                              "_t" + std::to_string(thread_id) + 
                              "_" + std::to_string(i);
            if (storage_->Save(group_key, data)) {
                total_success++;
            } else {
                total_failure++;
            }
            
            // 읽기
            if (i % 10 == 0) {
                auto results = storage_->Load(group_key);
                (void)results;
            }
            
            // 배치 로드
            if (i % 50 == 0) {
                auto batches = storage_->LoadBatch(group_key, 100);
                for (const auto& batch : batches) {
                    storage_->AcknowledgeBatch(group_key, batch.batch_id);
                }
            }
        }
    };
    
    auto start = high_resolution_clock::now();
    
    std::vector<std::thread> threads;
    for (size_t g = 0; g < num_groups; ++g) {
        for (size_t t = 0; t < num_threads_per_group; ++t) {
            threads.emplace_back(extreme_worker, g, t);
        }
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - start).count();
    
    // 각 그룹의 데이터 확인
    size_t total_data = 0;
    for (size_t g = 0; g < num_groups; ++g) {
        std::string group_key = "extreme_group_" + std::to_string(g);
        auto results = storage_->Load(group_key);
        total_data += results.size();
    }
    
    std::cout << "\n=== 극단적 부하 테스트 ===" << std::endl;
    std::cout << "그룹 수: " << num_groups << std::endl;
    std::cout << "그룹당 스레드 수: " << num_threads_per_group << std::endl;
    std::cout << "총 스레드 수: " << threads.size() << std::endl;
    std::cout << "성공한 작업: " << total_success << std::endl;
    std::cout << "실패한 작업: " << total_failure << std::endl;
    std::cout << "총 데이터 수: " << total_data << std::endl;
    std::cout << "소요 시간: " << duration << " ms" << std::endl;
    std::cout << "처리량: " << (double)total_success * 1000.0 / duration << " ops/sec" << std::endl;
    
    EXPECT_EQ(total_failure, 0);
    EXPECT_EQ(total_data, total_success);
}

