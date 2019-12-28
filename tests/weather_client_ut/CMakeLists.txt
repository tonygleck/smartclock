#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 3.2)

set(theseTestsName weather_client_ut)

include_directories(${PROJECT_SOURCE_DIR}/deps/parson ${PROJECT_SOURCE_DIR}/inc)
#target_include_directories(clock_util PUBLIC ${PROJECT_SOURCE_DIR}/inc)

set(${theseTestsName}_test_files
    ${theseTestsName}.c
)

set(${theseTestsName}_c_files
    ../../src/weather_client.c
    ../../deps/parson/parson.c
)

set(${theseTestsName}_h_files
)

#SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -E")
#SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -E")

build_test_project(${theseTestsName} "tests/smartclock_tests")

#build_code_coverage(${theseTestsName})