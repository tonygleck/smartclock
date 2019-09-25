
cmake_minimum_required(VERSION 3.3.0)

set(ntp_client_sample_files
    ntp_client_sample.c
)

add_executable(ntp_client_sample ${ntp_client_sample_files})

target_link_libraries(ntp_client_sample clock_util)
