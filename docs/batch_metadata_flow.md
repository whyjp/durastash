# 배치 메타데이터 처리 흐름

## 핵심 답변

**아니요, 배치 처리는 메모리의 BatchMetadata 구조체만으로 이루어지지 않습니다.**

실제로는 **메모리 구조체(BatchMetadata)와 RocksDB(영구 저장소) 간의 상호작용**으로 이루어집니다.

## 배치 처리 아키텍처

```
메모리 (BatchMetadata 객체)
    ↕ JSON 직렬화/역직렬화
RocksDB (영구 저장소)
```

## 상세 흐름

### 1. 배치 생성 (CreateBatch)

```cpp
// 1. 메모리에서 BatchMetadata 객체 생성
BatchMetadata metadata;
metadata.SetBatchId(batch_id);
metadata.SetStatus(BatchStatus::PENDING);
// ...

// 2. JSON으로 직렬화
std::string json_str = metadata.toJson();

// 3. RocksDB에 영구 저장
storage_->Put(key, json_str);
```

**결과:** 메타데이터가 RocksDB에 JSON 문자열로 저장됨

### 2. 배치 조회 (GetBatchMetadata)

```cpp
// 1. RocksDB에서 JSON 문자열 읽기
std::string json_str;
storage_->Get(key, json_str);

// 2. 메모리 객체로 역직렬화
BatchMetadata metadata;
metadata.fromJson(json_str);
```

**결과:** RocksDB의 JSON이 메모리 BatchMetadata 객체로 변환됨

### 3. 배치 상태 변경 (MarkBatchAsLoaded)

```cpp
// 1. RocksDB에서 현재 메타데이터 읽기
std::string json_str;
storage_->Get(key, json_str);

// 2. 메모리에서 상태 변경
BatchMetadata metadata;
metadata.fromJson(json_str);
metadata.SetStatus(BatchStatus::LOADED);  // 메모리에서 변경
metadata.SetLoadedAt(ULID::Now());

// 3. 변경된 상태를 다시 RocksDB에 저장
std::string updated_json = metadata.toJson();
storage_->Put(key, updated_json);
```

**결과:** 상태 변경이 RocksDB에 반영됨 (원자적 연산)

### 4. Load 가능한 배치 조회 (GetLoadableBatches)

```cpp
// 1. RocksDB에서 모든 배치 메타데이터 스캔
storage_->ScanPrefix(prefix, keys, values);

// 2. 각 메타데이터를 메모리 객체로 변환하여 필터링
for (size_t i = 0; i < keys.size(); ++i) {
    BatchMetadata metadata;
    metadata.fromJson(values[i]);  // JSON → 메모리 객체
    
    if (metadata.GetStatus() == BatchStatus::PENDING) {
        // PENDING 상태만 필터링
        pending_batches.push_back(...);
    }
}
```

**결과:** RocksDB에서 읽어서 메모리에서 필터링

### 5. 배치 ACK (AcknowledgeBatch)

```cpp
// 1. RocksDB에서 메타데이터 읽기
std::string json_str;
storage_->Get(key, json_str);
BatchMetadata metadata;
metadata.fromJson(json_str);

// 2. 메모리에서 데이터 키 생성
GenerateDataKeys(..., metadata.GetSequenceStart(), 
                 metadata.GetSequenceEnd(), data_keys);

// 3. RocksDB에서 메타데이터와 데이터 모두 삭제
storage_->DeleteFromBatch(metadata_key);
for (const auto& data_key : data_keys) {
    storage_->DeleteFromBatch(data_key);
}
```

**결과:** RocksDB에서 배치 관련 모든 데이터 삭제

## 왜 이렇게 설계되었나?

### 장점

1. **영구 저장**: 프로세스 재시작 후에도 배치 정보 유지
2. **프로세스 간 공유**: 여러 프로세스가 같은 배치를 처리 가능
3. **고가용성**: RocksDB의 트랜잭션 보장
4. **원자적 연산**: WriteBatch를 통한 일관성 보장

### 메모리만 사용한다면?

```cpp
// 나쁜 예: 메모리만 사용
std::map<std::string, BatchMetadata> batch_cache_;  // 프로세스 종료 시 손실!
```

**문제점:**
- 프로세스 재시작 시 배치 정보 손실
- 다른 프로세스와 배치 상태 공유 불가
- 고가용성 보장 불가

## 데이터 흐름 다이어그램

```
[Save 호출]
    ↓
[GroupStorage::Save]
    ↓
[BatchManager::CreateBatch]
    ↓
[메모리: BatchMetadata 객체 생성]
    ↓
[JSON 직렬화: metadata.toJson()]
    ↓
[RocksDB 저장: storage_->Put(key, json_str)]
    ↓
[영구 저장 완료]

[LoadBatch 호출]
    ↓
[BatchManager::GetLoadableBatches]
    ↓
[RocksDB 스캔: ScanPrefix]
    ↓
[각 메타데이터를 메모리 객체로 변환]
    ↓
[메모리에서 필터링: status == PENDING]
    ↓
[BatchManager::MarkBatchAsLoaded]
    ↓
[RocksDB에서 읽기 → 메모리에서 상태 변경 → RocksDB에 다시 저장]
    ↓
[LoadBatchData: 실제 데이터 로드]
    ↓
[결과 반환]
```

## 결론

**BatchMetadata는 메모리 구조체이지만, 실제 배치 처리는 RocksDB와의 상호작용으로 이루어집니다.**

- **메모리**: 일시적 처리, 빠른 접근
- **RocksDB**: 영구 저장, 프로세스 간 공유, 고가용성

이러한 하이브리드 접근 방식이 DuraStash의 핵심 설계입니다.

