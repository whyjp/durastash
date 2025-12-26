# 빌드 스크립트

## 개요

빌드 프로세스를 두 단계로 분리합니다:

1. **RocksDB 사전 빌드**: RocksDB 라이브러리를 별도로 빌드
2. **DuraStash 빌드**: 사전 빌드된 RocksDB를 사용하여 DuraStash 빌드

## 사용 방법

### 1단계: RocksDB 빌드

```powershell
# 정적 라이브러리로 빌드 (기본)
.\scripts\build_rocksdb.ps1

# 동적 라이브러리로 빌드
.\scripts\build_rocksdb.ps1 -LinkType Shared

# Debug 모드로 빌드
.\scripts\build_rocksdb.ps1 -BuildType Debug
```

빌드된 RocksDB는 `third_party\rocksdb_build`에 설치됩니다.

### 2단계: DuraStash 빌드

```powershell
# 사전 빌드된 RocksDB 사용
.\scripts\build.ps1 -UsePrebuiltRocksDB

# 서브모듈에서 직접 빌드 (기본 동작)
.\scripts\build.ps1
```

## 빌드 옵션

### RocksDB 빌드 옵션

- `-BuildType`: `Release`, `Debug`, `RelWithDebInfo` (기본: Release)
- `-LinkType`: `Static`, `Shared` (기본: Static)

### DuraStash 빌드 옵션

- `-BuildType`: `Release`, `Debug`, `RelWithDebInfo` (기본: Release)
- `-UsePrebuiltRocksDB`: 사전 빌드된 RocksDB 사용
- `-RocksDBRoot`: 사전 빌드된 RocksDB 경로 (기본: `third_party\rocksdb_build`)

## CMake 옵션

CMake를 직접 사용할 경우:

```powershell
# 사전 빌드된 RocksDB 사용
cmake .. -DUSE_PREBUILT_ROCKSDB=ON -DROCKSDB_ROOT=third_party\rocksdb_build

# 서브모듈에서 빌드 (기본)
cmake ..
```

## 장점

1. **빌드 시간 단축**: RocksDB는 한 번만 빌드하고 재사용
2. **독립성**: RocksDB와 DuraStash 빌드 분리
3. **유연성**: 정적/동적 링크 선택 가능
4. **재사용성**: 빌드된 RocksDB를 여러 프로젝트에서 사용 가능

