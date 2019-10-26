cmake_minimum_required(VERSION 3.3.0)

#Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(smartclock)

SET(CMAKE_INCLUDE_CURRENT_DIR ON)
set(PROJECT_INC_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/inc)
#set(TEST_INC_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/test/inc)

#option(use_opengl "set the smartclock to run on opengl (default is OFF)" OFF)
option(include_ut "Include unittest in build" OFF)
option(set_demo_mode "Set up demo mode" OFF)

include("${CMAKE_CURRENT_LIST_DIR}/cmake_configs/proj_config.cmake")

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

if (${set_demo_mode})
    add_definitions(-DDEMO_MODE)
endif()

# Setup dependencies
if (NOT TARGET azure_macro_utils_c)
    add_subdirectory(${PROJECT_SOURCE_DIR}/deps/macro-utils)
endif()
include_directories(${MACRO_UTILS_INC_FOLDER})

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deps/umock-c/CMakeLists.txt")
    execute_process(COMMAND git clone https://github.com/Azure/umock-c.git -q --no-tags ${CMAKE_CURRENT_SOURCE_DIR}/deps/umock-c)
endif()
add_subdirectory(${PROJECT_SOURCE_DIR}/deps/umock-c)
include_directories(${UMOCK_C_INC_FOLDER})

#if (NOT TARGET aziotsharedutil)
#    add_subdirectory(${PROJECT_SOURCE_DIR}/deps/c-utility)
#endif()
#include_directories(${SHARED_UTIL_INC_FOLDER})

if (NOT TARGET lib-util-c)
    add_subdirectory(${PROJECT_SOURCE_DIR}/deps/lib-util-c)
endif()
include_directories(${PROJECT_SOURCE_DIR}/deps/lib-util-c/inc)

if (NOT TARGET patchcords)
    add_subdirectory(${PROJECT_SOURCE_DIR}/deps/libpatchcords)
endif()
include_directories(${PROJECT_SOURCE_DIR}/deps/libpatchcords/inc)

#if (NOT TARGET uhttp)
#    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/uhttp)
#endif()
#include_directories(${UHTTP_C_INC_FOLDER})

set(include_ut ${original_include_ut})
include_directories(. inc ./deps/parson)

set(smartclock_test_files
    src/main.c
)

set(clockutil_src_files
    ${PROJECT_SOURCE_DIR}/src/ntp_client.c
    ${PROJECT_SOURCE_DIR}/src/alarm_scheduler.c
    #${PROJECT_SOURCE_DIR}/src/weather_client.c
    ${PROJECT_SOURCE_DIR}/deps/parson/parson.c
)

set(clockutil_h_files
    ${PROJECT_SOURCE_DIR}/inc/ntp_client.h
    ${PROJECT_SOURCE_DIR}/inc/alarm_scheduler.h
    #${PROJECT_SOURCE_DIR}/inc/weather_client.h
    ${PROJECT_SOURCE_DIR}/deps/parson/parson.h
)

add_library(clock_util SHARED ${clockutil_src_files} ${clockutil_h_files})

#target_link_libraries(clock_util uhttp)
#target_link_libraries(clock_util aziotsharedutil)
target_link_libraries(clock_util lib-util-c)
target_link_libraries(clock_util patchcords)
if (UNIX)
    target_link_libraries(clock_util cord_berkley)
endif (UNIX)


add_executable(smartclock_exe ${smartclock_test_files})
target_link_libraries(smartclock_exe clock_util asound patchcords)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/samples)

# Add testing
if (${include_ut})
    include("${CMAKE_CURRENT_LIST_DIR}/cmake_configs/proj_testing.cmake")
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


    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()

