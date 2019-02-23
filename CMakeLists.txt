cmake_minimum_required(VERSION 2.8.11)

#Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(smartclock)

SET(CMAKE_INCLUDE_CURRENT_DIR ON)
set(PROJECT_INC_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/inc)
#set(TEST_INC_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/test/inc)

option(include_ut "Include unittest in build" OFF)

include("cmake_configs/proj_config.cmake")

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

set(run_unittests OFF)
set(run_int_tests OFF)

include_directories(. inc ./deps)

# Setup dependencies
if (NOT TARGET umock-c)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/umock-c)
endif()
include_directories(${UMOCK_C_INC_FOLDER})

if (NOT TARGET lib-util-c)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/lib-util-c)
endif()
include_directories(${CMAKE_CURRENT_LIST_DIR}/deps/lib-util-c/inc)

###### Temporary
if (NOT TARGET aziotsharedutil)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/c-utility)
endif()
include_directories(${SHARED_UTIL_INC_FOLDER})
######

set(smartclock_src_files
    src/main.c
    src/ntp_client.c
    src/scheduler.c
)

set(smartclock_h_files
    inc/ntp_client.h
    inc/scheduler.h
)

add_executable(smartclock_exe ${smartclock_src_files} ${smartclock_h_files})
target_link_libraries(smartclock_exe aziotsharedutil)
target_link_libraries(smartclock_exe lib-util-c)

# Add testing
if (${include_ut})
    enable_testing()

    include("${CMAKE_CURRENT_LIST_DIR}/cmake_configs/proj_testing.cmake")

    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/testrunnerswitcher/CMakeLists.txt")
        execute_process(COMMAND git clone https://github.com/Azure/azure-c-testrunnerswitcher.git -q --no-tags ${CMAKE_CURRENT_SOURCE_DIR}/deps/testrunnerswitcher)
    endif()
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/ctest/CMakeLists.txt")
        execute_process(COMMAND git clone https://github.com/Azure/azure-ctest.git -q --no-tags ${CMAKE_CURRENT_SOURCE_DIR}/deps/ctest)
    endif()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/testrunnerswitcher)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/ctest)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()
