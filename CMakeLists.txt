cmake_minimum_required(VERSION 3.2)

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

if (${set_demo_mode})
    add_definitions(-DDEMO_MODE)
endif()

# Setup dependencies
if (NOT TARGET azure_macro_utils_c)
    add_subdirectory(${PROJECT_SOURCE_DIR}/deps/macro-utils)
    include_directories(${MACRO_UTILS_INC_FOLDER})
endif()

if (NOT TARGET umock_c)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/umock-c)
    include_directories(${UMOCK_C_INC_FOLDER})
endif()
if (NOT TARGET lib-util-c)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/lib-util-c)
    include_directories(${CMAKE_CURRENT_LIST_DIR}/deps/lib-util-c/inc/)
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
endif (UNIX)

add_executable(smartclock_exe ${smartclock_test_files})
target_link_libraries(smartclock_exe clock_util asound patchcords)
target_include_directories(smartclock_exe PRIVATE ./deps/parson)
target_include_directories(smartclock_exe PUBLIC ${PROJECT_SOURCE_DIR}/inc)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/samples)

# Add testing
if (${include_ut})

    include("${CMAKE_CURRENT_LIST_DIR}/cmake_configs/proj_testing.cmake")
    set(logging_files ${CMAKE_CURRENT_LIST_DIR}/deps/lib-util-c/src/app_logging.c)
    enable_testing()
    include (CTest)

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/azure-c-testrunnerswitcher)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps/azure-ctest)

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()

