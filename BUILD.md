# DuraStash 빌드 가이드

## 구현 완료된 컴포넌트

### 1. 핵심 컴포넌트

#### ULID 유틸리티 (`ulid.h/cpp`)
- 시간순 정렬 가능한 128비트 식별자 생성
- Base32 인코딩/디코딩
- 타임스탬프 추출 기능

#### 타입 시스템 (`types.h`)
- `BatchMetadata`: jsonable 기반 배치 메타데이터 구조체
- `SessionState`: jsonable 기반 세션 상태 구조체
- JSON 직렬화/역직렬화 자동 지원

#### 저장소 계층
- `IStorage`: 저장소 추상화 인터페이스 (DIP 준수)
- `RocksDBStorage`: RocksDB 기반 구현체
- 배치 쓰기, 범위 스캔, 접두사 검색 지원

#### 세션 관리 (`session_manager.h/cpp`)
- ULID 기반 세션 ID 발행
- 하트비트 스레드를 통한 세션 생존 확인
- 프로세스별 세션 격리
- 타임아웃 세션 자동 정리

#### 배치 관리 (`batch_manager.h/cpp`)
- 배치 생성 및 메타데이터 관리
- Load 상태 관리 (한번 Load된 배치는 재Load 불가)
- 배치 단위 ACK 처리 및 삭제
- FIFO 순서 보장

#### 그룹 저장소 (`group_storage.h/cpp`)
- 그룹 키 기반 데이터 분리
- 배치 단위 저장/로드
- Resave 기능 (부분 처리된 배치 재저장)
- 세션 자동 관리

### 2. 주요 기능

#### 데이터 흐름
```
Save → 배치 단위 그룹화 → RocksDB 저장
Load → 배치 상태 확인 → Load 가능한 배치만 반환 → 상태를 "loaded"로 변경
ACK → 배치 데이터 삭제 → 배치 메타데이터 삭제
Resave → 기존 배치 삭제 → 새로운 배치로 재저장
```

#### 동시성 보장
- **프로세스 내**: std::mutex 기반 fine-grained locking
- **프로세스 간**: RocksDB의 WriteBatch + 조건부 쓰기
- 배치 Load 시 원자적 상태 변경

#### 고가용성
- WriteBatch를 통한 원자적 쓰기
- 트랜잭션 기반 배치 처리
- 손상된 배치 감지 및 예외 처리
- 세션 타임아웃 처리

### 3. 테스트 코드

- `test_ulid.cpp`: ULID 생성 및 검증 테스트
- `test_types.cpp`: 타입 직렬화/역직렬화 테스트
- `test_group_storage.cpp`: GroupStorage 통합 테스트
  - 세션 초기화/종료
  - 데이터 저장/로드
  - 배치 ACK
  - Resave
  - FIFO 순서 보장
  - 다중 그룹 지원

## 빌드 요구사항

1. **CMake 3.15 이상**
   - Windows: https://cmake.org/download/
   - 또는 Visual Studio 2019 이상 (CMake 포함)

2. **C++17 컴파일러**
   - MSVC (Visual Studio)
   - MinGW-w64
   - Clang

3. **의존성**
   - RocksDB (서브모듈로 포함)
   - jsonable (서브모듈로 포함)

## 빌드 방법

### Windows (Visual Studio)

```powershell
# 서브모듈 초기화 (이미 완료됨)
git submodule update --init --recursive

# 빌드 디렉토리 생성
mkdir build
cd build

# CMake 구성 (Visual Studio 2019 이상)
cmake .. -G "Visual Studio 17 2022" -A x64

# 또는 기본 생성기 사용
cmake ..

# 빌드
cmake --build . --config Release

# 테스트 실행
ctest -C Release
```

### Windows (MinGW)

```powershell
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
ctest
```

### Linux/macOS

```bash
git submodule update --init --recursive
mkdir build && cd build
cmake ..
make
ctest
```

## 빌드 문제 해결

### CMake를 찾을 수 없는 경우

1. CMake 설치 확인:
   ```powershell
   # Chocolatey 사용
   choco install cmake
   
   # 또는 직접 다운로드
   # https://cmake.org/download/
   ```

2. PATH에 CMake 추가:
   ```powershell
   # 일반적으로 다음 경로에 설치됨
   C:\Program Files\CMake\bin
   ```

### RocksDB 빌드 오류

RocksDB는 자체 의존성이 많습니다:
- Windows: vcpkg 사용 권장
- Linux: 시스템 패키지 매니저 사용

### jsonable 헤더를 찾을 수 없는 경우

jsonable은 header-only 라이브러리이므로 빌드 불필요:
```powershell
# 서브모듈 확인
Test-Path third_party\jsonable\Jsonable.hpp
```

## 테스트 실행

빌드 성공 후:

```powershell
cd build
# 모든 테스트 실행
ctest

# 특정 테스트 실행
.\bin\Release\test_ulid.exe
.\bin\Release\test_types.exe
.\bin\Release\test_group_storage.exe
```

## 다음 단계

빌드가 성공하면:
1. 테스트 실행하여 기능 확인
2. 예제 코드 작성
3. 문서화 보완
4. 성능 테스트

