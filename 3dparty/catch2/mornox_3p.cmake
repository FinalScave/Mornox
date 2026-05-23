add_library(mornox_3p_catch2
    ${CMAKE_CURRENT_LIST_DIR}/src/catch_amalgamated.cpp
)

target_compile_features(mornox_3p_catch2 PUBLIC cxx_std_20)

target_include_directories(mornox_3p_catch2
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include/catch2
)

add_library(Mornox3p::Catch2 ALIAS mornox_3p_catch2)
