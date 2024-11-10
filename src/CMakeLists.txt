find_package(Threads)

add_library(${PROJECT_NAME}
    lockfree-hashtable.c
    lockfree-hashtable.h
)
set_target_properties(${PROJECT_NAME}
    PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS YES
)
target_link_libraries(${PROJECT_NAME} PUBLIC Threads::Threads)

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    target_compile_options(${PROJECT_NAME} PUBLIC "/experimental:c11atomics")
endif()
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
