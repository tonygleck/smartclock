#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

set(theseTestsName ntp_client_ut)

set(${theseTestsName}_test_files
    ${theseTestsName}.c
)

set(${theseTestsName}_c_files
    ../../src/ntp_client.c
)

set(${theseTestsName}_h_files
)

build_test_project(${theseTestsName} "tests/smartclock_tests")
