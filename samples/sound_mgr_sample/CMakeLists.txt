
cmake_minimum_required(VERSION 3.3.0)

set(sound_mgr_sample_files
sound_mgr_sample.c
)

add_executable(sound_mgr_sample ${sound_mgr_sample_files})

target_link_libraries(sound_mgr_sample clock_util)
