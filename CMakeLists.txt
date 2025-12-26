cmake_minimum_required(VERSION 3.15)
project(DuraStash VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 빌드 타입 설정
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 출력 디렉토리 설정
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 포함 디렉토리
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party)

include_directories(${INCLUDE_DIR})
include_directories(${THIRD_PARTY_DIR})
# jsonable이 rapidjson을 포함하고 있으므로 rapidjson 경로도 추가
include_directories(${THIRD_PARTY_DIR}/jsonable)
include_directories(${THIRD_PARTY_DIR}/jsonable/rapidjson/include)

# RocksDB 찾기 옵션
option(USE_PREBUILT_ROCKSDB "Use prebuilt RocksDB library instead of building from submodule" OFF)
set(ROCKSDB_ROOT "" CACHE PATH "Path to prebuilt RocksDB installation directory")
set(ROCKSDB_LIBRARY "" CACHE FILEPATH "Path to prebuilt RocksDB library file (alternative to ROCKSDB_ROOT)")
set(ROCKSDB_INCLUDE_DIR "" CACHE PATH "Path to prebuilt RocksDB include directory (alternative to ROCKSDB_ROOT)")

# RocksDB 찾기
if(USE_PREBUILT_ROCKSDB)
    # 방법 1: 라이브러리 파일과 헤더 디렉토리를 직접 지정
    if(ROCKSDB_LIBRARY AND ROCKSDB_INCLUDE_DIR)
        message(STATUS "Using prebuilt RocksDB library: ${ROCKSDB_LIBRARY}")
        message(STATUS "Using prebuilt RocksDB headers: ${ROCKSDB_INCLUDE_DIR}")
        add_library(rocksdb UNKNOWN IMPORTED)
        set_target_properties(rocksdb PROPERTIES
            IMPORTED_LOCATION "${ROCKSDB_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ROCKSDB_INCLUDE_DIR}"
        )
        set(rocksdb_FOUND TRUE)
    # 방법 2: 설치 디렉토리에서 찾기
    elseif(ROCKSDB_ROOT)
        message(STATUS "Using prebuilt RocksDB from: ${ROCKSDB_ROOT}")
        set(ROCKSDB_DIR ${ROCKSDB_ROOT})
        set(rocksdb_DIR ${ROCKSDB_ROOT}/lib/cmake/rocksdb)
        find_package(rocksdb REQUIRED PATHS ${rocksdb_DIR} NO_DEFAULT_PATH)
        
        if(NOT rocksdb_FOUND)
            # 수동으로 라이브러리 찾기
            find_library(ROCKSDB_LIBRARY
                NAMES rocksdb rocksdb-shared
                PATHS ${ROCKSDB_ROOT}/lib
                NO_DEFAULT_PATH
            )
            find_path(ROCKSDB_INCLUDE_DIR
                NAMES rocksdb/db.h
                PATHS ${ROCKSDB_ROOT}/include
                NO_DEFAULT_PATH
            )
            
            if(ROCKSDB_LIBRARY AND ROCKSDB_INCLUDE_DIR)
                message(STATUS "Found RocksDB library: ${ROCKSDB_LIBRARY}")
                message(STATUS "Found RocksDB headers: ${ROCKSDB_INCLUDE_DIR}")
                add_library(rocksdb UNKNOWN IMPORTED)
                set_target_properties(rocksdb PROPERTIES
                    IMPORTED_LOCATION "${ROCKSDB_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${ROCKSDB_INCLUDE_DIR}"
                )
                set(rocksdb_FOUND TRUE)
            else()
                message(FATAL_ERROR "Prebuilt RocksDB not found in ${ROCKSDB_ROOT}")
            endif()
        endif()
    else()
        message(FATAL_ERROR "USE_PREBUILT_ROCKSDB is ON but neither ROCKSDB_ROOT nor ROCKSDB_LIBRARY/ROCKSDB_INCLUDE_DIR are set")
    endif()
else()
    # 시스템 또는 서브모듈에서 찾기
    find_package(rocksdb QUIET)
    if(NOT rocksdb_FOUND)
        message(STATUS "RocksDB not found, using submodule")
        set(ROCKSDB_DIR ${THIRD_PARTY_DIR}/rocksdb)
        if(EXISTS ${ROCKSDB_DIR}/CMakeLists.txt)
            # RocksDB 테스트 비활성화하여 GTest 충돌 방지 (독립성 보장)
            set(WITH_TESTS OFF CACHE BOOL "Disable RocksDB tests" FORCE)
            add_subdirectory(${ROCKSDB_DIR} EXCLUDE_FROM_ALL)
        else()
            message(FATAL_ERROR "RocksDB submodule not found. Please run: git submodule update --init --recursive")
        endif()
    endif()
endif()

# jsonable 헤더 전용 라이브러리
set(JSONABLE_DIR ${THIRD_PARTY_DIR}/jsonable)
if(NOT EXISTS ${JSONABLE_DIR}/Jsonable.hpp)
    message(WARNING "jsonable not found in third_party/jsonable")
    message(STATUS "Please clone jsonable: git submodule add https://github.com/whyjp/jsonable.git third_party/jsonable")
endif()

# 소스 파일
set(SOURCES
    src/storage.cpp
    src/rocksdb_storage.cpp
    src/group_storage.cpp
    src/session_manager.cpp
    src/batch_manager.cpp
    src/ulid.cpp
)

set(HEADERS
    include/durastash/storage.h
    include/durastash/rocksdb_storage.h
    include/durastash/group_storage.h
    include/durastash/session_manager.h
    include/durastash/batch_manager.h
    include/durastash/types.h
    include/durastash/ulid.h
    include/durastash/errors.h
)

# 라이브러리 생성
add_library(durastash STATIC ${SOURCES} ${HEADERS})

target_include_directories(durastash PUBLIC
    $<BUILD_INTERFACE:${INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(durastash PUBLIC
    rocksdb
)

# Windows 시스템 라이브러리 링크 (RocksDB가 필요로 함)
if(WIN32)
    target_link_libraries(durastash PUBLIC rpcrt4 shlwapi)
endif()

# 컴파일 옵션
if(MSVC)
    # MSVC 병렬 빌드 시 PDB 파일 충돌 방지 (전역 설정)
    add_compile_options(/FS)
    target_compile_options(durastash PRIVATE /W4 /utf-8)
    # UTF-8 BOM 설정을 위한 옵션
    set_source_files_properties(${SOURCES} PROPERTIES COMPILE_FLAGS "/utf-8")
else()
    target_compile_options(durastash PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 테스트 빌드 옵션
option(BUILD_TESTS "Build tests" ON)

if(BUILD_TESTS)
    enable_testing()
    
    # GoogleTest 찾기 - 독립적으로 FetchContent 사용 (RocksDB와 독립성 보장)
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found, using FetchContent")
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
        )
        # RocksDB가 이미 GTest를 빌드했는지 확인
        if(NOT TARGET gtest)
            FetchContent_MakeAvailable(googletest)
        else()
            message(STATUS "GTest already available from RocksDB")
        endif()
    endif()
    
    # 테스트 파일
    file(GLOB TEST_SOURCES tests/*.cpp)
    
    foreach(TEST_FILE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
        add_executable(${TEST_NAME} ${TEST_FILE})
        # 타겟 이름 확인하여 적절한 라이브러리 링크
        if(TARGET gtest)
            target_link_libraries(${TEST_NAME} PRIVATE durastash gtest gtest_main)
        elseif(TARGET GTest::gtest)
            target_link_libraries(${TEST_NAME} PRIVATE durastash GTest::gtest GTest::gtest_main)
        else()
            message(FATAL_ERROR "GTest target not found")
        endif()
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
    endforeach()
endif()

# 설치 설정
install(TARGETS durastash
    EXPORT durastashTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

# rocksdb가 서브모듈로 빌드된 경우 export set에 포함
if(TARGET rocksdb AND NOT rocksdb_FOUND)
    install(TARGETS rocksdb
        EXPORT durastashTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )
endif()

install(DIRECTORY include/ DESTINATION include)

install(EXPORT durastashTargets
    FILE durastashTargets.cmake
    NAMESPACE durastash::
    DESTINATION lib/cmake/durastash
)

