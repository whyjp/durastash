@echo off
REM DuraStash 전체 빌드 및 테스트 스크립트
REM Usage: build_and_test.bat [Release|Debug]

REM UTF-8 코드 페이지 설정 (한글 출력 깨짐 방지)
chcp 65001 >nul 2>&1

setlocal enabledelayedexpansion

set BUILD_TYPE=Release

REM 파라미터 파싱
if not "%~1"=="" set BUILD_TYPE=%~1

set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."

echo ========================================
echo DuraStash 전체 빌드 및 테스트
echo ========================================
echo 빌드 타입: %BUILD_TYPE%
echo.

REM 1. RocksDB 빌드
echo ========================================
echo 1단계: RocksDB 빌드
echo ========================================
call "%SCRIPT_DIR%build_rocksdb.bat" %BUILD_TYPE% Static
if errorlevel 1 (
    echo 오류: RocksDB 빌드 실패
    exit /b 1
)

echo.
echo.

REM 2. DuraStash 빌드
echo ========================================
echo 2단계: DuraStash 빌드
echo ========================================
call "%SCRIPT_DIR%build.bat" %BUILD_TYPE% UsePrebuiltRocksDB
if errorlevel 1 (
    echo 오류: DuraStash 빌드 실패
    exit /b 1
)

echo.
echo.

REM 3. 테스트 실행
echo ========================================
echo 3단계: 테스트 실행
echo ========================================
call "%SCRIPT_DIR%run_tests.bat" %BUILD_TYPE%
if errorlevel 1 (
    echo 오류: 테스트 실패
    exit /b 1
)

echo.
echo ========================================
echo 모든 빌드 및 테스트 완료!
echo ========================================

endlocal

