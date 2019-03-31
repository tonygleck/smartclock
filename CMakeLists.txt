cmake_minimum_required(VERSION 2.8.11)

#Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(smartclock)

SET(CMAKE_INCLUDE_CURRENT_DIR ON)
set(PROJECT_INC_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/inc)
#set(TEST_INC_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/test/inc)

#option(use_opengl "set the smartclock to run on opengl (default is OFF)" OFF)
option(include_ut "Include unittest in build" OFF)

enable_testing()

include("${CMAKE_CURRENT_LIST_DIR}/cmake_configs/proj_config.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake_configs/proj_testing.cmake")

#if (MSVC)
#    detect_architecture("_M_AMD64" x86_64)
#    detect_architecture("_M_IX86" x86)
#    detect_architecture("_M_ARM" ARM)
#else()
#    detect_architecture("__x86_64__" x86_64)
#    detect_architecture("__i386__" x86)
#    detect_architecture("__arm__" ARM)
#endif()
#if (NOT DEFINED ARCHITECTURE OR ARCHITECTURE STREQUAL "")
#    set(ARCHITECTURE "GENERIC")
#endif()
#message(STATUS "Current architecture: ${ARCHITECTURE}")

if(MSVC)
    # Force to always compile with W4
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /wd4201")

    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W4")
    add_definitions(/WX)

    add_definitions(-D_CRT_SECURE_NO_WARNINGS)

    #set_source_files_properties(bandit/bandit/external/optionparser.h PROPERTIES COMPILE_FLAGS "/wd4701 /wd4703")
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    # Update if necessary
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -Wno-variadic-macros")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -pedantic -Wno-variadic-macros")
    # Make warning as error
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif()

set(original_include_ut ${include_ut})

set(run_e2e_tests OFF)
set(run_unittests OFF)
set(skip_samples ON)
set(use_http ON)
set(include_ut OFF)

include_directories(. inc ./deps)

# Setup dependencies
if (NOT TARGET aziotsharedutil)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/c-utility)
endif()
include_directories(${SHARED_UTIL_INC_FOLDER})

if (NOT TARGET umock-c)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/umock-c)
endif()
include_directories(${UMOCK_C_INC_FOLDER})

if (NOT TARGET lib-util-c)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/lib-util-c)
endif()
include_directories(${CMAKE_CURRENT_LIST_DIR}/deps/lib-util-c/inc)

if (NOT TARGET uhttp)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/uhttp)
endif()
include_directories(${UHTTP_C_INC_FOLDER})

set(include_ut ${original_include_ut})

set(smartclock_test_files
    src/main.c
)

set(clockutil_src_files
    src/ntp_client.c
    src/alarm_scheduler.c
    src/weather_client.c
)

set(clockutil_h_files
    inc/ntp_client.h
    inc/alarm_scheduler.h
    inc/weather_client.h
)

add_library(clock_util ${clockutil_src_files} ${clockutil_h_files})
target_link_libraries(clock_util uhttp)
target_link_libraries(clock_util aziotsharedutil)
target_link_libraries(clock_util lib-util-c)

add_executable(smartclock_exe ${smartclock_test_files})
target_link_libraries(smartclock_exe clock_util)

# Add testing
if (${include_ut})
    set(logging_files ${CMAKE_CURRENT_LIST_DIR}/deps/lib-util-c/src/app_logging.c)
    enable_testing()

    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/testrunnerswitcher/CMakeLists.txt")
        execute_process(COMMAND git clone https://github.com/Azure/azure-c-testrunnerswitcher.git -q --no-tags ${CMAKE_CURRENT_SOURCE_DIR}/deps/testrunnerswitcher)
    endif()
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/ctest/CMakeLists.txt")
        execute_process(COMMAND git clone https://github.com/Azure/azure-ctest.git -q --no-tags ${CMAKE_CURRENT_SOURCE_DIR}/deps/ctest)
    endif()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/testrunnerswitcher)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/ctest)
endif()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
