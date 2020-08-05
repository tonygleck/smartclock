cmake_minimum_required(VERSION 3.2)

#Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(smartclock)

SET(CMAKE_INCLUDE_CURRENT_DIR ON)
set(PROJECT_INC_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/inc)

#option(use_opengl "set the smartclock to run on opengl (default is OFF)" OFF)
option(smartclock_ut "Include unittest in build" OFF)
option(smartclock_samples "Include samples in build" OFF)
option(smartclock_gui "Build the gui using SDL2" OFF)

if (CMAKE_BUILD_TYPE MATCHES "Debug" AND NOT WIN32)
    set(DEBUG_CONFIG ON)
    set(ENABLE_COVERAGE ON)
    add_definitions(-DSMARTCLOCK_DEBUG)
else()
    set(ENABLE_COVERAGE OFF)
    set(DEBUG_CONFIG OFF)
endif()

include("${PROJECT_SOURCE_DIR}/cmake_configs/proj_config.cmake")

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
    add_subdirectory(${PROJECT_SOURCE_DIR}/deps/patchcords)
    include_directories(${PROJECT_SOURCE_DIR}/deps/patchcords/inc)
endif()
if (NOT TARGET http_client)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/deps/http_client)
    include_directories(${PROJECT_SOURCE_DIR}/deps/http_client/inc)
endif()

if (${smartclock_gui})
    if (NOT TARGET lvgl)
        add_subdirectory(${PROJECT_SOURCE_DIR}/deps/gui)
        include_directories(${PROJECT_SOURCE_DIR}/deps/gui)
    endif()
endif()

set(smartclock_include_files
    ${PROJECT_SOURCE_DIR}/inc/config_mgr.h
    ${PROJECT_SOURCE_DIR}/inc/gui_mgr.h
    ${PROJECT_SOURCE_DIR}/inc/smartclock.h
    ${PROJECT_SOURCE_DIR}/deps/parson/parson.h
)
set(smartclock_src_files
    ${PROJECT_SOURCE_DIR}/src/main.c
    ${PROJECT_SOURCE_DIR}/src/config_mgr.c
    ${PROJECT_SOURCE_DIR}/src/smartclock.c
    ${PROJECT_SOURCE_DIR}/deps/parson/parson.c
)

set(clockutil_src_files
    ${PROJECT_SOURCE_DIR}/deps/parson/parson.c
    ${PROJECT_SOURCE_DIR}/src/alarm_scheduler.c
    ${PROJECT_SOURCE_DIR}/src/ntp_client.c
    ${PROJECT_SOURCE_DIR}/src/sound_mgr_openal.c
    ${PROJECT_SOURCE_DIR}/src/weather_client.c
)
set(clockutil_h_files
    ${PROJECT_SOURCE_DIR}/deps/parson/parson.h
    ${PROJECT_SOURCE_DIR}/inc/alarm_scheduler.h
    ${PROJECT_SOURCE_DIR}/inc/ntp_client.h
    ${PROJECT_SOURCE_DIR}/inc/sound_mgr.h
    ${PROJECT_SOURCE_DIR}/inc/time_mgr.h
    #${PROJECT_SOURCE_DIR}/inc/system_config.h
    ${PROJECT_SOURCE_DIR}/inc/weather_client.h
)


if (WIN32)
else()
    set(clockutil_src_files
        ${clockutil_src_files}
            #${PROJECT_SOURCE_DIR}/src/system_config.c
            ${PROJECT_SOURCE_DIR}/src/pal/linux/time_mgr_linux.c
        )
endif()

if (${smartclock_gui})
    set(smartclock_src_files
        ${smartclock_src_files}
        ${PROJECT_SOURCE_DIR}/src/gui_mgr_lvgl.c

        # Add the Font files
        ${PROJECT_SOURCE_DIR}/fonts/arial_20.c

        ${PROJECT_SOURCE_DIR}/media/img/alarm_img.c

        # Add the Image files
        ${PROJECT_SOURCE_DIR}/media/img/zero_img.c
        ${PROJECT_SOURCE_DIR}/media/img/one_img.c
        ${PROJECT_SOURCE_DIR}/media/img/two_img.c
        ${PROJECT_SOURCE_DIR}/media/img/three_img.c
        ${PROJECT_SOURCE_DIR}/media/img/four_img.c
        ${PROJECT_SOURCE_DIR}/media/img/five_img.c
        ${PROJECT_SOURCE_DIR}/media/img/six_img.c
        ${PROJECT_SOURCE_DIR}/media/img/seven_img.c
        ${PROJECT_SOURCE_DIR}/media/img/eight_img.c
        ${PROJECT_SOURCE_DIR}/media/img/nine_img.c
        ${PROJECT_SOURCE_DIR}/media/img/ellipse_img.c
        ${PROJECT_SOURCE_DIR}/media/img/blank_img.c

        ${PROJECT_SOURCE_DIR}/media/img/cloudy_img.c
        ${PROJECT_SOURCE_DIR}/media/img/light_rain_img.c
        ${PROJECT_SOURCE_DIR}/media/img/partly_sunny_img.c
        ${PROJECT_SOURCE_DIR}/media/img/rain_showers_img.c
        ${PROJECT_SOURCE_DIR}/media/img/sunny_img.c
    )
else()
    set(smartclock_src_files
        ${smartclock_src_files}
        ${PROJECT_SOURCE_DIR}/src/gui_mgr_cmd.c
    )
endif()

add_library(clock_util SHARED ${clockutil_src_files} ${clockutil_h_files})
target_include_directories(clock_util PRIVATE ./deps/parson)
target_include_directories(clock_util PUBLIC ${PROJECT_SOURCE_DIR}/inc)

target_link_libraries(clock_util
    PUBLIC lib-util-c patchcords http_client)

if (WIN32)
else() # Linux
    target_link_libraries(clock_util
        PUBLIC openal cord_berkley)
endif()

smartclock_addCompileSettings(clock_util)
compileTargetAsC99(clock_util)

add_executable(smartclock_exe ${smartclock_src_files})
target_include_directories(smartclock_exe PUBLIC ${PROJECT_SOURCE_DIR}/deps/parson)
target_link_libraries(smartclock_exe clock_util lib-util-c patchcords)

if (${smartclock_gui})
    target_link_libraries(smartclock_exe lvgl SDL2)
else()
    target_link_libraries(smartclock_exe ncurses)
    target_link_libraries(smartclock_exe m)
endif()

target_include_directories(smartclock_exe PRIVATE ./deps/parson)
target_include_directories(smartclock_exe PUBLIC ${PROJECT_SOURCE_DIR}/inc)

if (${smartclock_samples})
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/samples)
endif()

smartclock_addCompileSettings(smartclock_exe)
compileTargetAsC99(smartclock_exe)

# Add testing
if (${smartclock_ut})

    include("${CMAKE_CURRENT_LIST_DIR}/cmake_configs/proj_testing.cmake")
    enable_coverage_testing()

    if ((NOT TARGET ctest) AND (EXISTS ${CMAKE_CURRENT_LIST_DIR}/deps/azure-ctest/CMakeLists.txt))
        add_subdirectory(${PROJECT_SOURCE_DIR}/deps/azure-ctest)
    endif()
    include_directories(${CTEST_INC_FOLDER})

    enable_testing()
    include (CTest)

    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif()

