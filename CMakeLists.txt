cmake_minimum_required(VERSION 3.2)

#Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(smartclock)

SET(CMAKE_INCLUDE_CURRENT_DIR ON)
set(PROJECT_INC_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/inc)
#set(TEST_INC_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/test/inc)

#option(use_opengl "set the smartclock to run on opengl (default is OFF)" OFF)
option(smartclock_ut "Include unittest in build" OFF)
option(set_demo_mode "Set up demo mode" OFF)

if (CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT WIN32)
    set(DEBUG_CONFIG ON)
    set(ENABLE_COVERAGE ON)
else()
    set(ENABLE_COVERAGE OFF)
    set(DEBUG_CONFIG OFF)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cmake_configs/proj_config.cmake")

if (${set_demo_mode})
    add_definitions(-DDEMO_MODE)
endif()

# Setup dependencies
if (NOT TARGET azure_macro_utils_c)
    add_subdirectory(${PROJECT_SOURCE_DIR}/deps/macro-utils)
    include_directories(${MACRO_UTILS_INC_FOLDER})
endif()
if (NOT TARGET umock_c)
    add_subdirectory(${PROJECT_SOURCE_DIR}/deps/umock-c)
    include_directories(${UMOCK_C_INC_FOLDER})
endif()
if (NOT TARGET lib-util-c)
    add_subdirectory(${PROJECT_SOURCE_DIR}/deps/lib-util-c)
    include_directories(${PROJECT_SOURCE_DIR}/deps/lib-util-c/inc/)
endif()
if (NOT TARGET patchcords)
    add_subdirectory(${PROJECT_SOURCE_DIR}/deps/libpatchcords)
    include_directories(${PROJECT_SOURCE_DIR}/deps/libpatchcords/inc)
endif()
if (NOT TARGET http_client)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/http_client)
    include_directories(${PROJECT_SOURCE_DIR}/deps/http_client/inc)
endif()

set(smartclock_test_files
    src/main.c
)

set(clockutil_src_files
    ${PROJECT_SOURCE_DIR}/deps/parson/parson.c
    ${PROJECT_SOURCE_DIR}/src/alarm_scheduler.c
    ${PROJECT_SOURCE_DIR}/src/ntp_client.c
    ${PROJECT_SOURCE_DIR}/src/sound_mgr_openal.c
    #${PROJECT_SOURCE_DIR}/src/system_config.c
    ${PROJECT_SOURCE_DIR}/src/weather_client.c
)

set(clockutil_h_files
    ${PROJECT_SOURCE_DIR}/deps/parson/parson.h
    ${PROJECT_SOURCE_DIR}/inc/alarm_scheduler.h
    ${PROJECT_SOURCE_DIR}/inc/ntp_client.h
    ${PROJECT_SOURCE_DIR}/inc/sound_mgr.h
    #${PROJECT_SOURCE_DIR}/inc/system_config.h
    ${PROJECT_SOURCE_DIR}/inc/weather_client.h
)

add_library(clock_util SHARED ${clockutil_src_files} ${clockutil_h_files})
target_include_directories(clock_util PRIVATE ./deps/parson)
target_include_directories(clock_util PUBLIC ${PROJECT_SOURCE_DIR}/inc)

target_link_libraries(clock_util lib-util-c)
target_link_libraries(clock_util patchcords)
target_link_libraries(clock_util http_client)
target_link_libraries(clock_util openal)
if (UNIX)
    target_link_libraries(clock_util cord_berkley)
else()
endif (UNIX)

addCompileSettings(clock_util)
compileTargetAsC99(clock_util)

add_executable(smartclock_exe ${smartclock_test_files})
target_link_libraries(smartclock_exe clock_util asound patchcords)
target_include_directories(smartclock_exe PRIVATE ./deps/parson)
target_include_directories(smartclock_exe PUBLIC ${PROJECT_SOURCE_DIR}/inc)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/samples)

addCompileSettings(smartclock_exe)
compileTargetAsC99(smartclock_exe)

# Add testing
if (${smartclock_ut})

    include("${CMAKE_CURRENT_LIST_DIR}/cmake_configs/proj_testing.cmake")
    enable_coverage_testing()

    if ((NOT TARGET testrunnerswitcher) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-c-testrunnerswitcher/CMakeLists.txt))
        add_subdirectory(${PROJECT_SOURCE_DIR}/deps/azure-c-testrunnerswitcher)
    endif()
    include_directories(${TESTRUNNERSWITCHER_INC_FOLDER})
    if ((NOT TARGET ctest) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-ctest/CMakeLists.txt))
        add_subdirectory(${PROJECT_SOURCE_DIR}/deps/azure-ctest)
    endif()
    include_directories(${CTEST_INC_FOLDER})

    enable_testing()
    include (CTest)

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()

