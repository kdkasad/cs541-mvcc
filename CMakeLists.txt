cmake_minimum_required(VERSION 3.28)
project(cs541_mvcc VERSION 0.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# For mutexes (and threads once we get to that)
find_package(Threads REQUIRED)

add_library(cs541_mvcc src/store.cpp src/transaction.cpp src/version.cpp)
target_include_directories(cs541_mvcc PUBLIC include)
target_compile_features(cs541_mvcc PUBLIC cxx_std_23)
target_link_libraries(cs541_mvcc PUBLIC Threads::Threads)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cs541_mvcc PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(cs541_mvcc_demo src/main.cpp)
target_link_libraries(cs541_mvcc_demo PRIVATE cs541_mvcc)

add_executable(cs541_mvcc_bench bench/main.cpp bench/workload.cpp bench/report.cpp)
target_link_libraries(cs541_mvcc_bench PRIVATE cs541_mvcc)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(cs541_mvcc_bench PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(cs541_mvcc_tests tests/tests.cpp)
    target_link_libraries(cs541_mvcc_tests PRIVATE cs541_mvcc)
    foreach(test_name IN ITEMS
        simple_read_existing
        simple_read_missing
        read_ignores_earlier_uncommitted_write
        read_ignores_later_uncommitted_write
        simple_scan
        read_after_commit
        read_before_and_after_commit
        conflicting_writes_abort_second_writer
        aborted_write_is_ignored
        write_after_aborted_write_succeeds
        non_repeatable_read_is_aborted
        version_created_and_invalidated_within_read_transaction
        non_conflicting_writes_both_commit
        read_stale_version_from_aborted_writer
        parallel_reads_partitioned_keys
        parallel_writes_partitioned_keys
        repeated_writes
    )
        add_test(NAME ${test_name} COMMAND cs541_mvcc_tests ${test_name})
        set_tests_properties(${test_name} PROPERTIES TIMEOUT 10)
    endforeach()
endif()
