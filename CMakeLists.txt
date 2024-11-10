cmake_minimum_required(VERSION 3.26)
project(lockfree-hashtable LANGUAGES CXX C VERSION 0.0.1)

include(FetchContent)
enable_testing()

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)

FetchContent_MakeAvailable(Catch2)

add_subdirectory(src)
add_subdirectory(test)
