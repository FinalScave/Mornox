add_library(vanta_3p_catch2
    ${CMAKE_CURRENT_LIST_DIR}/src/catch_amalgamated.cpp
)

target_compile_features(vanta_3p_catch2 PUBLIC cxx_std_20)

target_include_directories(vanta_3p_catch2
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/include/catch2
)

add_library(Vanta3p::Catch2 ALIAS vanta_3p_catch2)
