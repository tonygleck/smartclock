
cmake_minimum_required(VERSION 3.3.0)

set(weather_client_files
    weather_client_sample.c
)

add_executable(weather_client_sample ${weather_client_files})

target_link_libraries(weather_client_sample clock_util)
