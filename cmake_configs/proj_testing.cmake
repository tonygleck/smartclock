function(build_catch_test_project whatIsBuilding folder)
    include_directories(${PROJECT_INC_DIRECTORY} ${TEST_INC_DIRECTORY})

    if (WIN32)
        add_definitions(-DUNICODE)
        add_definitions(-D_UNICODE)
        #windows needs this define
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)

        add_executable(${whatIsBuilding} 
            ${${whatIsBuilding}_cpp_files}
            ${${whatIsBuilding}_h_files}
            #../../testtools/inc/catch.hpp
        )
        set_target_properties(${whatIsBuilding} PROPERTIES LINKER_LANGUAGE CXX)
        set_target_properties(${whatIsBuilding} PROPERTIES FOLDER ${folder})
    endif()


endfunction()

function(add_unittest_directory whatIsBuilding)
    add_subdirectory(${whatIsBuilding})
    add_test(NAME ${whatIsBuilding} COMMAND ${whatIsBuilding})

    if (WIN32)
    else()
        add_test(NAME ${whatIsBuilding}_valgrind COMMAND valgrind --num-callers=10 --error-exitcode=1 --leak-check=full --track-origins=yes $TARGET_FILE:${whatIsBuilding})
    endif()
endfunction(add_unittest_directory)

function(build_test_project whatIsBuilding folder)
    include_directories(${PROJECT_INC_DIRECTORY} ${TEST_INC_DIRECTORY})
    include_directories(${BANDIT_UNITTEST_DIR})

    if (WIN32)
        add_definitions(-DUNICODE)
        add_definitions(-D_UNICODE)
        #windows needs this define
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    
        #link_directories(${whatIsBuilding} $ENV{VCInstallDir}UnitTest/lib)
        #target_include_directories(${whatIsBuilding} $ENV{VCInstallDir}UnitTest/include)
        #include_directories($ENV{VCInstallDir}UnitTest/include)

        add_executable(${whatIsBuilding} 
            ${${whatIsBuilding}_cpp_files}
            ${${whatIsBuilding}_h_files}
            ${${whatIsBuilding}_c_files}
        )

        #add_library(${whatIsBuilding} SHARED
        #    ${${whatIsBuilding}_cpp_files}
        #    ${${whatIsBuilding}_h_files}
        #    ${${whatIsBuilding}_c_files}
        #)
        set_target_properties(${whatIsBuilding} PROPERTIES FOLDER ${folder})
    else()
        add_executable(${whatIsBuilding}
            ${${whatIsBuilding}_cpp_files}
            ${${whatIsBuilding}_h_files}
            ${${whatIsBuilding}_c_files}
        )
    endif()
    set_property(TARGET ${whatIsBuilding} PROPERTY CXX_STANDARD 14)
endfunction()
