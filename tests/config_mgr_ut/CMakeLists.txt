#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

set(theseTestsName config_mgr_ut)

include_directories(${PROJECT_SOURCE_DIR}/deps/parson ${PROJECT_SOURCE_DIR}/inc)

set(${theseTestsName}_test_files
    ${theseTestsName}.c
)

set(${theseTestsName}_c_files
    ../../src/config_mgr.c
)

set(${theseTestsName}_h_files
)

build_test_project(${theseTestsName} "tests/smartclock_tests")
