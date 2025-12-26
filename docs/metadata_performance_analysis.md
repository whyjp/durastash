# 메타데이터 처리 병목 분석 및 개선 방안

## 현재 병목 지점

### 1. GetLoadableBatches - 전체 스캔 문제

**현재 구현:**
```cpp
// 모든 배치 메타데이터를 스캔
storage_->ScanPrefix(prefix, keys, values);

// 메모리에서 필터링
for (size_t i = 0; i < keys.size(); ++i) {
    BatchMetadata metadata;
    metadata.fromJson(values[i]);  // 모든 배치를 역직렬화
    if (metadata.GetStatus() == BatchStatus::PENDING) {
        // 필터링
    }
}
```

**문제점:**
- 배치가 많을수록 모든 메타데이터를 읽어야 함 (O(n))
- JSON 역직렬화 오버헤드가 누적됨
- mutex 잠금 시간이 길어짐

**예상 성능:**
- 배치 100개: ~1-2ms
- 배치 1,000개: ~10-20ms
- 배치 10,000개: ~100-200ms ⚠️

### 2. MarkBatchAsLoaded - 개별 읽기/쓰기

**현재 구현:**
```cpp
// 1. 개별 Get 호출
storage_->Get(key, json_str);

// 2. JSON 역직렬화
metadata.fromJson(json_str);

// 3. 상태 변경
metadata.SetStatus(BatchStatus::LOADED);

// 4. JSON 직렬화
updated_json = metadata.toJson();

// 5. 개별 Put 호출
storage_->Put(key, updated_json);
```

**문제점:**
- 각 배치마다 2번의 I/O (Get + Put)
- JSON 직렬화/역직렬화 오버헤드
- mutex 잠금이 각 호출마다 발생

### 3. ScanPrefix - 전체 데이터 로드

**현재 구현:**
```cpp
// 모든 키와 값을 메모리로 로드
for (it->Seek(prefix); it->Valid(); it->Next()) {
    keys.push_back(it->key().ToString());
    values.push_back(it->value().ToString());  // 전체 JSON 로드
}
```

**문제점:**
- 배치가 많을수록 메모리 사용량 증가
- 불필요한 데이터까지 메모리로 로드

## 개선 방안

### 방안 1: 인덱스 구조 추가 (권장)

**아이디어:** PENDING 상태 배치만 별도 인덱스로 관리

```cpp
// 인덱스 키: group_key:session_id:pending:sequence_start:batch_id
// 메타데이터 키: group_key:session_id:batch:batch_id

// 배치 생성 시
void CreateBatch(...) {
    // 메타데이터 저장
    storage_->Put(metadata_key, json_str);
    
    // PENDING 인덱스에 추가
    std::string index_key = MakePendingIndexKey(group_key, session_id, 
                                                 sequence_start, batch_id);
    storage_->Put(index_key, "");  // 값은 불필요
}

// Load 가능한 배치 조회
size_t GetLoadableBatches(...) {
    // PENDING 인덱스만 스캔 (훨씬 빠름)
    std::string prefix = group_key + ":" + session_id + ":pending:";
    storage_->ScanPrefix(prefix, keys, values);
    
    // 이미 정렬되어 있음 (sequence_start 포함)
    // 메타데이터 역직렬화 불필요!
}
```

**장점:**
- PENDING 배치만 스캔 (LOADED 배치는 제외)
- JSON 역직렬화 불필요 (인덱스만 읽음)
- 성능 향상: O(n) → O(k) (k = PENDING 배치 수)

**단점:**
- 인덱스 유지 오버헤드
- 상태 변경 시 인덱스도 업데이트 필요

### 방안 2: 배치 상태 변경 최적화

**아이디어:** WriteBatch를 사용하여 원자적 업데이트

```cpp
bool MarkBatchAsLoaded(...) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // WriteBatch 사용
    storage_->BeginBatch();
    
    // 메타데이터 업데이트
    std::string key = MakeBatchMetadataKey(...);
    BatchMetadata metadata;
    // ... 상태 변경
    storage_->PutToBatch(key, metadata.toJson());
    
    // 인덱스에서 제거
    std::string index_key = MakePendingIndexKey(...);
    storage_->DeleteFromBatch(index_key);
    
    // 원자적 커밋
    return storage_->CommitBatch();
}
```

**장점:**
- 원자적 연산 보장
- I/O 횟수 감소 (Get + Put → 1번의 WriteBatch)

### 방안 3: 메타데이터 캐싱

**아이디어:** 자주 접근하는 메타데이터를 메모리에 캐싱

```cpp
class BatchManager {
private:
    // LRU 캐시
    std::unordered_map<std::string, BatchMetadata> metadata_cache_;
    size_t cache_size_limit_ = 1000;
    
    bool GetBatchMetadata(...) {
        // 캐시 확인
        auto it = metadata_cache_.find(key);
        if (it != metadata_cache_.end()) {
            metadata = it->second;
            return true;
        }
        
        // RocksDB에서 읽기
        // ... 캐시에 저장
    }
};
```

**장점:**
- 반복 접근 시 성능 향상
- RocksDB I/O 감소

**단점:**
- 메모리 사용량 증가
- 캐시 일관성 관리 필요

### 방안 4: 스캔 최적화 (조기 종료)

**아이디어:** 필요한 개수만 읽고 종료

```cpp
size_t GetLoadableBatches(..., size_t batch_size, ...) {
    // 필요한 개수만 읽기
    size_t count = 0;
    for (it->Seek(prefix); it->Valid() && count < batch_size; it->Next()) {
        // 빠른 필터링 (JSON 역직렬화 전)
        if (IsPendingStatus(it->value())) {  // 간단한 문자열 검사
            batch_ids.push_back(ExtractBatchId(it->key()));
            count++;
        }
    }
}
```

**장점:**
- 불필요한 데이터 읽기 방지
- 메모리 사용량 감소

## 성능 비교 예상

| 시나리오 | 현재 | 방안 1 (인덱스) | 방안 2 (배치) | 방안 3 (캐시) |
|---------|------|----------------|--------------|--------------|
| 배치 100개, PENDING 10개 | ~2ms | ~0.2ms | ~1ms | ~0.5ms |
| 배치 1,000개, PENDING 50개 | ~20ms | ~1ms | ~5ms | ~2ms |
| 배치 10,000개, PENDING 100개 | ~200ms | ~2ms | ~10ms | ~5ms |

## 권장 개선 순서

1. **1단계: 인덱스 구조 추가** (가장 큰 성능 향상)
2. **2단계: 배치 상태 변경 최적화** (원자성 + 성능)
3. **3단계: 스캔 최적화** (메모리 사용량 감소)
4. **4단계: 선택적 캐싱** (필요 시)

## 구현 우선순위

**높은 우선순위:**
- GetLoadableBatches 최적화 (가장 자주 호출됨)
- MarkBatchAsLoaded 배치 처리 (원자성 보장)

**중간 우선순위:**
- 스캔 조기 종료
- 인덱스 구조 추가

**낮은 우선순위:**
- 메타데이터 캐싱 (필요 시 추가)

