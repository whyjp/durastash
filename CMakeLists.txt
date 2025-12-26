cmake_minimum_required(VERSION 3.15)
project(DuraStash VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
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

# RocksDB 찾기
find_package(rocksdb QUIET)
if(NOT rocksdb_FOUND)
    message(STATUS "RocksDB not found, using submodule")
    set(ROCKSDB_DIR ${THIRD_PARTY_DIR}/rocksdb)
    if(EXISTS ${ROCKSDB_DIR}/CMakeLists.txt)
        add_subdirectory(${ROCKSDB_DIR})
    else()
        message(FATAL_ERROR "RocksDB submodule not found. Please run: git submodule update --init --recursive")
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
    rocksdb::rocksdb
)

# 컴파일 옵션
if(MSVC)
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
    
    # GoogleTest 찾기
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        message(STATUS "GTest not found, using FetchContent")
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
        )
        FetchContent_MakeAvailable(googletest)
    endif()
    
    # 테스트 파일
    file(GLOB TEST_SOURCES tests/*.cpp)
    
    foreach(TEST_FILE ${TEST_SOURCES})
        get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
        add_executable(${TEST_NAME} ${TEST_FILE})
        target_link_libraries(${TEST_NAME} PRIVATE durastash GTest::gtest GTest::gtest_main)
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

install(DIRECTORY include/ DESTINATION include)

install(EXPORT durastashTargets
    FILE durastashTargets.cmake
    NAMESPACE durastash::
    DESTINATION lib/cmake/durastash
)

