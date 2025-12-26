# Load 인터페이스 분석

## 현재 Load 인터페이스

### 제공되는 인터페이스

**GroupStorage 레벨:**
```cpp
// 배치 단위 로드만 제공
std::vector<BatchLoadResult> LoadBatch(const std::string& group_key, size_t batch_size);
```

**특징:**
- ✅ 배치 단위로만 로드 가능
- ✅ 한번 Load된 배치는 재Load 불가 (상태가 LOADED로 변경됨)
- ✅ FIFO 순서 보장
- ✅ 배치 ACK 기반 설계 (Load → ACK → 삭제)

### 제공되지 않는 인터페이스

**개별 아이템 로드:**
```cpp
// ❌ 없음
std::string Load(const std::string& group_key, int64_t sequence_id);
std::vector<std::string> LoadRange(const std::string& group_key, 
                                   int64_t start_seq, int64_t end_seq);
```

**Peek (읽기만, 상태 변경 없음):**
```cpp
// ❌ 없음
std::vector<BatchLoadResult> PeekBatch(const std::string& group_key, size_t batch_size);
```

**조건부 로드:**
```cpp
// ❌ 없음
std::vector<BatchLoadResult> LoadBatchIf(const std::string& group_key, 
                                         std::function<bool(const BatchMetadata&)> predicate);
```

## 현재 설계 철학

### 배치 ACK 기반 설계

```
Save → 배치 생성 (PENDING)
  ↓
LoadBatch → 배치 로드 (PENDING → LOADED)
  ↓
AcknowledgeBatch → 배치 삭제 (ACK)
```

**특징:**
1. **한번 Load된 배치는 재Load 불가**
   - `MarkBatchAsLoaded`로 상태 변경
   - 중복 처리 방지

2. **명시적 ACK 필요**
   - Load만으로는 삭제되지 않음
   - ACK 후에야 삭제됨

3. **배치 단위 처리만 지원**
   - 개별 아이템 로드 불가
   - 배치 전체를 로드해야 함

## 설계 의도 분석

### 장점

1. **메시지 큐 패턴**
   - Producer-Consumer 모델
   - 한번 처리된 메시지는 재처리 방지

2. **트랜잭션 보장**
   - 배치 단위로 원자적 처리
   - 부분 실패 시 Resave 가능

3. **순서 보장**
   - FIFO 순서 보장
   - 배치 단위로 순서 유지

### 제한사항

1. **개별 아이템 접근 불가**
   - 특정 sequence_id의 데이터만 읽을 수 없음
   - 배치 전체를 로드해야 함

2. **Peek 기능 없음**
   - 데이터를 읽기만 하고 상태를 변경하지 않을 수 없음
   - Load = 상태 변경 (LOADED)

3. **조건부 로드 불가**
   - 특정 조건의 배치만 로드할 수 없음
   - 모든 PENDING 배치를 스캔해야 함

## 사용 사례별 적합성

### ✅ 적합한 사용 사례

1. **메시지 큐**
   ```
   Producer → Save → Consumer → LoadBatch → Process → ACK
   ```

2. **이벤트 스트리밍**
   ```
   Event → Save → Processor → LoadBatch → Handle → ACK
   ```

3. **작업 큐**
   ```
   Task → Save → Worker → LoadBatch → Execute → ACK
   ```

### ❌ 부적합한 사용 사례

1. **키-값 저장소**
   ```
   // 개별 아이템 접근 필요
   Get(group_key, sequence_id);  // ❌ 없음
   ```

2. **읽기 전용 조회**
   ```
   // Peek 기능 필요
   PeekBatch(group_key);  // ❌ 없음
   ```

3. **조건부 조회**
   ```
   // 특정 조건의 배치만 로드
   LoadBatchIf(group_key, predicate);  // ❌ 없음
   ```

## 개선 제안

### Option 1: Peek 인터페이스 추가

```cpp
/**
 * 배치 Peek (읽기만, 상태 변경 없음)
 * @param group_key 그룹 키
 * @param batch_size 배치 크기
 * @return 배치 로드 결과 목록 (상태 변경 없음)
 */
std::vector<BatchLoadResult> PeekBatch(const std::string& group_key, size_t batch_size);
```

**용도:** 데이터 확인만 하고 상태를 변경하지 않을 때

### Option 2: 개별 아이템 로드 추가

```cpp
/**
 * 개별 아이템 로드 (sequence_id 기반)
 * @param group_key 그룹 키
 * @param sequence_id 시퀀스 ID
 * @return 데이터 (없으면 빈 문자열)
 */
std::string Load(const std::string& group_key, int64_t sequence_id);
```

**용도:** 특정 아이템만 읽어야 할 때

### Option 3: 범위 로드 추가

```cpp
/**
 * 범위 로드 (sequence_id 범위)
 * @param group_key 그룹 키
 * @param start_seq 시작 시퀀스
 * @param end_seq 종료 시퀀스
 * @return 데이터 목록
 */
std::vector<std::string> LoadRange(const std::string& group_key, 
                                   int64_t start_seq, int64_t end_seq);
```

**용도:** 특정 범위의 데이터만 읽어야 할 때

## 결론

**현재는 배치 ACK 기반 인터페이스만 제공됩니다.**

- ✅ `LoadBatch`: 배치 단위 로드 (상태 변경)
- ❌ 개별 아이템 로드 없음
- ❌ Peek 기능 없음
- ❌ 조건부 로드 없음

이는 **메시지 큐/작업 큐 패턴**에 최적화된 설계입니다. 다른 사용 사례가 필요하다면 추가 인터페이스가 필요할 수 있습니다.

