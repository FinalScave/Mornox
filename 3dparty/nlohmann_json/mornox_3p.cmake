add_library(mornox_3p_nlohmann_json INTERFACE)

target_compile_features(mornox_3p_nlohmann_json INTERFACE cxx_std_20)

target_include_directories(mornox_3p_nlohmann_json
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

add_library(Mornox3p::NlohmannJson ALIAS mornox_3p_nlohmann_json)
