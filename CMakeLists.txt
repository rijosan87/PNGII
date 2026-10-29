# PNG Icon Injector - CMake Build Configuration
cmake_minimum_required(VERSION 3.21)

project(PNGIconInjector VERSION 0.1.0.3 LANGUAGES CXX)

# Output folder configuration
if(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data")
endif()

if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}")
endif()

# Project-local pinned release so other plugins keep their own CommonLib version.
# Setup: git clone --depth 1 --branch v12.1.0 https://github.com/alandtse/CommonLibSSE-NG.git external/CommonLibSSE-NG
if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/external/CommonLibSSE-NG")
    set(CLIB_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/CommonLibSSE-NG")
elseif(DEFINED ENV{CommonLibSSEPath})
    file(TO_CMAKE_PATH "$ENV{CommonLibSSEPath}" CLIB_SOURCE_DIR)
endif()

if(NOT CLIB_SOURCE_DIR OR NOT EXISTS "${CLIB_SOURCE_DIR}/cmake/CommonLibSSE.cmake")
    message(FATAL_ERROR "CommonLibSSE-NG 12.1.0 source not found. Clone it into external/CommonLibSSE-NG.")
endif()

file(READ "${CLIB_SOURCE_DIR}/vcpkg.json" CLIB_MANIFEST)
string(JSON CLIB_VERSION GET "${CLIB_MANIFEST}" "version-semver")
if(NOT CLIB_VERSION VERSION_EQUAL "12.1.0")
    message(FATAL_ERROR "PNGIconInjector requires CommonLibSSE-NG 12.1.0; found ${CLIB_VERSION} at ${CLIB_SOURCE_DIR}.")
endif()
message(STATUS "CommonLibSSE-NG ${CLIB_VERSION}: ${CLIB_SOURCE_DIR}")

# Must be set before add_subdirectory: option() inside NG cannot override an existing cache entry.
set(COMMONLIB_PREBUILT OFF CACHE BOOL "" FORCE)
set(COMMONLIB_PREBUILT_DIR "" CACHE PATH "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
add_subdirectory("${CLIB_SOURCE_DIR}" CommonLibSSE-NG)
# NG only includes this on its prebuilt path, which returns early.
include("${CLIB_SOURCE_DIR}/cmake/CommonLibSSE.cmake")

# JSON icon rules
find_package(nlohmann_json CONFIG REQUIRED)
# TextureManager: PNG decode + mip chain (same package Party Sheet uses)
find_package(directxtex CONFIG REQUIRED)
# TextureManager: SVG icons rasterized at 2x the drawn size, then the PNG mip route
find_package(lunasvg CONFIG REQUIRED)

add_commonlibsse_plugin(${PROJECT_NAME}
    MINIMUM_SKSE_VERSION 2.0.17
    SOURCES
        src/plugin.cpp
        src/CategoryIcons.cpp
        src/LootIcons.cpp
        src/QuickLootIcons.cpp
        src/IconAPI.cpp
        src/IconLabels.cpp
        src/ASBridge.cpp
        src/IconRules.cpp
        src/NewItems.cpp
        src/RowIcons.cpp
        src/StatusIcons.cpp
        src/TCCItemStatus.cpp
        src/TextureManager.cpp
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_precompile_headers(${PROJECT_NAME} PRIVATE includes/PCH.h)

# Always emit a matching PDB, even on Release, so crash-log offsets can be resolved later.
# /OPT:REF /OPT:ICF keep the release binary optimized; debug info and optimization coexist.
# The PDB is archived (below), not shipped with the mod.
target_compile_options(${PROJECT_NAME} PRIVATE /Zi)
target_link_options(${PROJECT_NAME} PRIVATE /DEBUG /OPT:REF /OPT:ICF)
target_include_directories(${PROJECT_NAME} PRIVATE includes)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        nlohmann_json::nlohmann_json
        Microsoft::DirectXTex
        lunasvg::lunasvg
)

target_compile_definitions(${PROJECT_NAME} PRIVATE _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING)

# Copy only the DLL to the MO2 mod folder. Loose assets (INI, JSON, PNGs) are edited there directly;
# copying Data/ on every build overwrote the deployed INI and any rule or PNG edits made in MO2.
if(DEFINED OUTPUT_FOLDER)
    set(DLL_FOLDER "${OUTPUT_FOLDER}/SKSE/Plugins")

    message(STATUS "SKSE plugin output folder: ${DLL_FOLDER}")

    add_custom_command(
        TARGET "${PROJECT_NAME}"
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${DLL_FOLDER}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        VERBATIM
    )

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(
            TARGET "${PROJECT_NAME}"
            POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DLL_FOLDER}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
            VERBATIM
        )
    endif()
endif()

# Archive the PDB for every build into a version-stamped folder in the source tree, not the
# deployed mod folder. Never ship these in the public release zip.
# Layout: pdb-archive/<project-version>/PNGIconInjector.pdb, with the matching DLL alongside
# so the GUID/age pair stays together.
set(PDB_ARCHIVE "${CMAKE_SOURCE_DIR}/pdb-archive/${PROJECT_VERSION}")
add_custom_command(
    TARGET "${PROJECT_NAME}"
    POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PDB_ARCHIVE}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${PDB_ARCHIVE}/$<TARGET_PDB_FILE_NAME:${PROJECT_NAME}>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${PDB_ARCHIVE}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    VERBATIM
)
