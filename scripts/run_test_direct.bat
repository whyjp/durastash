@echo off
REM DuraStash 테스트 직접 실행 스크립트 (인코딩 설정 포함)
REM Usage: run_test_direct.bat [test_name] [Release|Debug]

REM UTF-8 코드 페이지 설정 (한글 출력 깨짐 방지)
chcp 65001 >nul 2>&1

setlocal enabledelayedexpansion

set BUILD_TYPE=Release
set TEST_NAME=

REM 파라미터 파싱
if not "%~1"=="" set TEST_NAME=%~1
if not "%~2"=="" set BUILD_TYPE=%~2

REM 프로젝트 루트 디렉토리
set "SCRIPT_DIR=%~dp0"
set "PROJECT_ROOT=%SCRIPT_DIR%.."
set "BIN_DIR=%PROJECT_ROOT%\build\bin\%BUILD_TYPE%"

REM 테스트 이름이 지정되지 않은 경우 사용법 출력
if "%TEST_NAME%"=="" (
    echo 사용법: run_test_direct.bat [test_name] [Release^|Debug]
    echo.
    echo 사용 가능한 테스트:
    echo   - test_group_storage
    echo   - test_performance
    echo   - test_types
    echo   - test_ulid
    echo.
    exit /b 1
)

REM 실행 파일 경로
set "TEST_EXE=%BIN_DIR%\%TEST_NAME%.exe"

REM 실행 파일 존재 확인
if not exist "%TEST_EXE%" (
    echo 오류: 테스트 실행 파일을 찾을 수 없습니다: %TEST_EXE%
    echo 먼저 빌드하세요: scripts\build.bat %BUILD_TYPE%
    exit /b 1
)

echo === 테스트 실행: %TEST_NAME% ===
echo 빌드 타입: %BUILD_TYPE%
echo 실행 파일: %TEST_EXE%
echo.

REM 환경 변수 설정
set PYTHONIOENCODING=utf-8
set LANG=ko_KR.UTF-8

REM 테스트 실행
"%TEST_EXE%" %*

REM 종료 코드 전달
set EXIT_CODE=%ERRORLEVEL%
if %EXIT_CODE% NEQ 0 (
    echo.
    echo 테스트 실패 (종료 코드: %EXIT_CODE%)
    exit /b %EXIT_CODE%
)

echo.
echo === 테스트 완료 ===

endlocal

