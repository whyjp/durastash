# DuraStash

RocksDB 기반 그룹별 save-load 저장소 라이브러리

## 개요

DuraStash는 로그 전송 및 임시 백업을 위한 고가용성 저장소 라이브러리입니다. 
그룹별 데이터 분리, FIFO 순서 보장, 배치 단위 처리, 프로세스 세션 관리 등의 기능을 제공합니다.

## 주요 기능

- **그룹별 데이터 분리**: 문자열 키 기반 그룹별 데이터 관리
- **FIFO 순서 보장**: SequenceID 기반 순차 처리
- **배치 단위 처리**: 배치 단위 Load 및 ACK 처리
- **프로세스 세션 관리**: ULID 기반 세션 생명주기 관리
- **동시성 보장**: 프로세스 내/간 동시성 제어
- **고가용성**: 원자적 쓰기, 트랜잭션 기반 처리

## 빌드 요구사항

- C++17 이상
- CMake 3.15 이상
- RocksDB
- jsonable (rapidjson 기반)

## 빌드 방법

### 방법 1: 두 단계 빌드 (권장)

RocksDB를 별도로 빌드한 후 DuraStash를 빌드합니다:

```powershell
# 1단계: RocksDB 사전 빌드
.\scripts\build_rocksdb.ps1

# 2단계: DuraStash 빌드 (사전 빌드된 RocksDB 사용)
.\scripts\build.ps1 -UsePrebuiltRocksDB

# 테스트 실행
cd build
ctest -C Release
```

### 방법 2: 서브모듈에서 직접 빌드

```powershell
# 서브모듈 초기화
git submodule update --init --recursive

# 빌드 디렉토리 생성
mkdir build
cd build

# CMake 구성
cmake .. -G "Visual Studio 18 2026" -A x64

# 빌드
cmake --build . --config Release

# 테스트 실행 (선택사항)
ctest -C Release
```

자세한 내용은 [scripts/README.md](scripts/README.md)를 참조하세요.

## 사용 예제

```cpp
#include <durastash/group_storage.h>

// 저장소 생성 및 초기화
durastash::GroupStorage storage("/path/to/db");
storage.Initialize();

// 세션 초기화
std::string group_key = "error_log";
storage.InitializeSession(group_key);

// 데이터 저장
storage.Save(group_key, "로그 데이터");

// 배치 단위 로드
auto batches = storage.LoadBatch(group_key, 100);
if (!batches.empty()) {
    auto& batch = batches[0];
    // 배치 처리...
    
    // 배치 ACK
    storage.AcknowledgeBatch(group_key, batch.batch_id);
}

// 세션 종료
storage.TerminateSession(group_key);
storage.Shutdown();
```

## 라이센스

MIT License

