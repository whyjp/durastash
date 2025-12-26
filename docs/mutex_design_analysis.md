# Mutex 설계 분석: Recursive Mutex vs 불필요한 레이어 제거

## 현재 구조

```
GroupStorage (mutex_)
  └─> BatchManager (mutex_)
       └─> RocksDBStorage (mutex_)
```

각 레벨이 자신의 mutex를 가지고 있어 중첩 잠금이 발생합니다.

## 접근 방식 비교

### 1. Recursive Mutex 사용

**장점:**
- ✅ 중첩 호출이 자연스럽게 가능
- ✅ 기존 코드 구조 유지 가능
- ✅ 각 레벨의 독립성 보장

**단점:**
- ❌ **성능 오버헤드**: recursive mutex는 일반 mutex보다 느림 (약 10-20% 성능 저하)
- ❌ **디버깅 어려움**: 실제로 중첩 잠금이 필요한지, 설계 오류인지 판단 어려움
- ❌ **데드락 위험 증가**: recursive mutex는 다른 mutex와 함께 사용 시 데드락 가능성 증가
- ❌ **코드 냄새**: 중첩 잠금은 보통 설계 문제의 신호

**예시:**
```cpp
// BatchManager
std::recursive_mutex mutex_;  // 성능 저하, 디버깅 어려움
```

### 2. 불필요한 Mutex 레이어 제거 (현재 해결책)

**장점:**
- ✅ **성능 향상**: 불필요한 잠금 제거로 성능 개선
- ✅ **명확한 설계**: 각 레벨의 책임이 명확해짐
- ✅ **데드락 위험 감소**: 잠금 계층이 단순해짐
- ✅ **유지보수 용이**: 코드 이해와 디버깅이 쉬워짐

**단점:**
- ⚠️ 호출 체인을 신중하게 설계해야 함
- ⚠️ 이미 잠긴 상태에서 호출되는 메서드와 그렇지 않은 메서드를 구분해야 함

**현재 해결책:**
```cpp
// AcknowledgeBatch 내부에서 GetBatchMetadata를 직접 구현
// (이미 mutex가 잠긴 상태이므로 중첩 잠금 불필요)
bool BatchManager::AcknowledgeBatch(...) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // GetBatchMetadata 호출 대신 직접 구현
    std::string key = MakeBatchMetadataKey(...);
    std::string json_str;
    if (!storage_->Get(key, json_str)) {
        return false;
    }
    // ...
}
```

## 권장 접근 방식

### 원칙: "최소 잠금 원칙" (Minimal Locking Principle)

1. **각 레벨의 책임 분석:**
   - `GroupStorage`: 그룹 세션 관리, 시퀀스 카운터 관리
   - `BatchManager`: 배치 메타데이터 관리, 배치 상태 관리
   - `RocksDBStorage`: 실제 DB I/O, WriteBatch 관리

2. **잠금이 필요한 경우:**
   - 여러 스레드에서 동시 접근 가능한 공유 상태
   - 원자적 연산이 필요한 경우

3. **잠금이 불필요한 경우:**
   - 이미 상위 레벨에서 잠금이 보장된 경우
   - 단순히 하위 레벨의 기능을 호출하는 경우

### 개선 제안

#### Option A: 내부 헬퍼 메서드 분리 (현재 방식)
```cpp
// Public API (mutex 잠금)
bool GetBatchMetadata(...) {
    std::lock_guard<std::mutex> lock(mutex_);
    return GetBatchMetadataImpl(...);
}

// Internal helper (mutex 잠금 없음)
bool GetBatchMetadataImpl(...) {
    // 실제 구현
}

// 내부 호출 시
bool AcknowledgeBatch(...) {
    std::lock_guard<std::mutex> lock(mutex_);
    BatchMetadata metadata;
    GetBatchMetadataImpl(...);  // mutex 없이 호출
}
```

#### Option B: 잠금 계층 재설계
```cpp
// GroupStorage만 mutex 보유
// BatchManager와 RocksDBStorage는 mutex 제거
// (GroupStorage가 모든 동시성 제어)
```

## 성능 비교

| 접근 방식 | 성능 | 복잡도 | 유지보수성 | 데드락 위험 |
|---------|------|--------|----------|------------|
| Recursive Mutex | ⭐⭐ | ⭐⭐⭐ | ⭐⭐ | ⭐⭐⭐ |
| 불필요한 레이어 제거 | ⭐⭐⭐ | ⭐⭐ | ⭐⭐⭐ | ⭐ |

## 결론

**불필요한 mutex 레이어를 제거하는 것이 더 효과적입니다.**

이유:
1. **성능**: 불필요한 잠금 제거로 성능 향상
2. **명확성**: 설계 의도가 명확해짐
3. **안전성**: 데드락 위험 감소
4. **유지보수**: 코드 이해와 디버깅 용이

Recursive mutex는 "빠른 해결책"이지만 근본적인 설계 문제를 숨기는 코드 냄새입니다.

