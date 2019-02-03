cmake_minimum_required(VERSION 2.8.11)

#Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(smartclock)

SET(CMAKE_INCLUDE_CURRENT_DIR ON)
set(PROJECT_INC_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/inc)
#set(TEST_INC_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/test/inc)

option(use_opengl "set the smartclock to run on opengl (default is OFF)" OFF)
option(include_ut "Include unittest in build" OFF)

enable_testing()

include("cmake_configs/proj_config.cmake")

if (MSVC)
    detect_architecture("_M_AMD64" x86_64)
    detect_architecture("_M_IX86" x86)
    detect_architecture("_M_ARM" ARM)
else()
    detect_architecture("__x86_64__" x86_64)
    detect_architecture("__i386__" x86)
    detect_architecture("__arm__" ARM)
endif()
if (NOT DEFINED ARCHITECTURE OR ARCHITECTURE STREQUAL "")
    set(ARCHITECTURE "GENERIC")
endif()
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

    #set_source_files_properties(bandit/bandit/external/optionparser.h PROPERTIES COMPILE_FLAGS "/wd4701 /wd4703")
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
    # Update if necessary
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -Wno-variadic-macros")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -pedantic -Wno-variadic-macros")
    # Make warning as error
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif()

include_directories(. inc ./deps)

# Setup dependencies
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/umock-c)
include_directories(${UMOCK_C_INC_FOLDER})

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/lib-util-c)
include_directories(${CMAKE_CURRENT_LIST_DIR}/deps/lib-util-c/inc)

###### Temporary
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/c-utility)
include_directories(${SHARED_UTIL_INC_FOLDER})
######

set(smartclock_src_files
    src/main.c
    src/ntp_client.c
)

set(smartclock_h_files
    inc/ntp_client.h
)

add_executable(smartclock ${smartclock_src_files} ${smartclock_h_files})
target_link_libraries(smartclock aziotsharedutil)
target_link_libraries(smartclock lib-util-c)

# Add testing
if (${include_ut})
    include("${CMAKE_CURRENT_LIST_DIR}/deps/lib-util-c/cmake_configs/proj_test.cmake")

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
