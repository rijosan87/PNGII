cmake_minimum_required(VERSION 3.21)
project(PNGIIAPITests LANGUAGES CXX)

enable_testing()

add_executable(pngii_api_contract api_contract.cpp ../src/IconAPI.cpp)
target_compile_features(pngii_api_contract PRIVATE cxx_std_17)
target_include_directories(pngii_api_contract PRIVATE ../includes)
add_test(NAME api_contract COMMAND pngii_api_contract)

if(WIN32)
    add_library(pngii_api_fixture SHARED api_fixture.cpp ../src/IconAPI.cpp)
    target_compile_features(pngii_api_fixture PRIVATE cxx_std_17)
    target_include_directories(pngii_api_fixture PRIVATE ../includes)

    add_executable(pngii_api_export api_export.cpp)
    target_compile_features(pngii_api_export PRIVATE cxx_std_17)
    target_include_directories(pngii_api_export PRIVATE ../includes)
    if(MSVC)
        set_property(TARGET pngii_api_fixture PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        set_property(TARGET pngii_api_export PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
    add_test(NAME api_export COMMAND pngii_api_export $<TARGET_FILE:pngii_api_fixture>)
endif()
