add_executable(${PROJECT_NAME}-test
    basic.cpp
)
set_target_properties(${PROJECT_NAME}-test
    PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS YES
        C_STANDARD 11
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS YES
)
target_link_libraries(${PROJECT_NAME}-test
    PUBLIC
        ${PROJECT_NAME}
    PRIVATE
        Catch2::Catch2WithMain
)
add_test(NAME basics COMMAND ${PROJECT_NAME}-test)
install(TARGETS ${PROJECT_NAME}-test RUNTIME DESTINATION bin)
