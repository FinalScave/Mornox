add_library(vanta_3p_nlohmann_json INTERFACE)

target_compile_features(vanta_3p_nlohmann_json INTERFACE cxx_std_20)

target_include_directories(vanta_3p_nlohmann_json
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
)

add_library(Vanta3p::NlohmannJson ALIAS vanta_3p_nlohmann_json)
