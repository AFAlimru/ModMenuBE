cmake_minimum_required(VERSION 3.22)

project(ModMenuBE_Android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(MOD_ID "ModMenuBE")
set(MOD_NAME "ModMenuBE")
set(MOD_AUTHOR "AFAlimru")
set(MOD_VERSION "0.2.0-alpha.1")
set(MOD_LIBRARY_NAME "ModMenuBE")
set(MOD_MINECRAFT_VERSIONS "[\"1.26.*\"]")

if(NOT ANDROID)
    message(FATAL_ERROR "Configure with the Android NDK toolchain (see scripts/build.ps1)")
endif()

include(FetchContent)

FetchContent_Declare(
    preloader_android
    GIT_REPOSITORY https://github.com/LiteLDev/preloader-android.git
    # The C++ structs (ModuleInfo, ...) must match the preloader inside LeviLauncher. Launcher 1.6.1 ships
    # main @ cc08b40 (ModuleInfo has onKeybind); 0.2.3 is one field short and crashes registerModule.
    GIT_TAG cc08b40a31471d3e2dedb267f5f17b28da2b0604
    SOURCE_SUBDIR cmake/preloader-sdk
)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_Declare(
    fmt
    URL https://github.com/fmtlib/fmt/archive/refs/tags/11.2.0.tar.gz
)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 013ac3beddff3dbffafd5177e7972067cd2b5083
)
FetchContent_Declare(
    imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9.tar.gz
)
set(FMT_DOC OFF CACHE BOOL "" FORCE)
set(FMT_TEST OFF CACHE BOOL "" FORCE)
set(FMT_INSTALL OFF CACHE BOOL "" FORCE)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(preloader_android nlohmann_json fmt imgui stb)

FetchContent_GetProperties(preloader_android)
FetchContent_GetProperties(imgui)
FetchContent_GetProperties(stb)

option(LEVI_LINK_PRELOADER "Build and link the preloader target" ON)
if(LEVI_LINK_PRELOADER AND NOT TARGET preloader)
    add_subdirectory("${preloader_android_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_preloader_android")
endif()

add_library(imgui_gles STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(imgui_gles PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")
target_compile_definitions(imgui_gles PUBLIC IMGUI_IMPL_OPENGL_ES3)
target_compile_options(imgui_gles PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)

set(SHARED_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../src")
add_library(${MOD_LIBRARY_NAME} SHARED
    src/main.cpp
    src/ModMenuMod.cpp
    src/EglOverlay.cpp
    src/TouchInput.cpp
    src/Entry.cpp
    src/ImeFix.cpp
    src/platform/Jni.cpp
    src/platform/AndroidHttp.cpp
    src/platform/AndroidPlatform.cpp
    src/platform/AndroidIconCache.cpp
    # shared with the Windows build
    ${SHARED_SRC}/api/ExportedApi.cpp
    ${SHARED_SRC}/core/Adapter.cpp
    ${SHARED_SRC}/core/ConfigRegistry.cpp
    ${SHARED_SRC}/core/I18n.cpp
    ${SHARED_SRC}/core/JsonConfigDoc.cpp
    ${SHARED_SRC}/core/LipCatalog.cpp
    ${SHARED_SRC}/core/ModRegistry.cpp
    ${SHARED_SRC}/core/Settings.cpp
    ${SHARED_SRC}/core/UpdateChecker.cpp
    ${SHARED_SRC}/core/Util.cpp
    ${SHARED_SRC}/ui/Fonts.cpp
    ${SHARED_SRC}/ui/Markdown.cpp
    ${SHARED_SRC}/ui/ModMenuScreen.cpp
    ${SHARED_SRC}/ui/OreTheme.cpp
    ${SHARED_SRC}/ui/SelfConfig.cpp
)

target_include_directories(${MOD_LIBRARY_NAME} PRIVATE
    src
    "${SHARED_SRC}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    "${preloader_android_SOURCE_DIR}/include"
    "${stb_SOURCE_DIR}"
)
target_compile_definitions(${MOD_LIBRARY_NAME} PRIVATE MODMENU_BUILDING)

target_compile_options(${MOD_LIBRARY_NAME} PRIVATE
    -fvisibility=hidden -fvisibility-inlines-hidden -Wall -Wno-unused-parameter -Wno-unused-function
)

target_link_options(${MOD_LIBRARY_NAME} PRIVATE "-Wl,-z,max-page-size=16384")

target_link_libraries(${MOD_LIBRARY_NAME} PRIVATE
    imgui_gles
    log android EGL GLESv3
    nlohmann_json::nlohmann_json
    fmt::fmt
)

if(TARGET preloader)
    target_link_libraries(${MOD_LIBRARY_NAME} PRIVATE preloader)
else()
    target_link_options(${MOD_LIBRARY_NAME} PRIVATE "-Wl,--unresolved-symbols=ignore-all")
endif()

add_custom_command(TARGET ${MOD_LIBRARY_NAME} POST_BUILD
    COMMAND "${CMAKE_STRIP}" --strip-unneeded "$<TARGET_FILE:${MOD_LIBRARY_NAME}>"
    VERBATIM
)

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/manifest.json.in" "${CMAKE_CURRENT_BINARY_DIR}/manifest.json" @ONLY)

set(PKG_ROOT "${CMAKE_CURRENT_BINARY_DIR}/package")
set(PKG_DIR "${PKG_ROOT}/${MOD_ID}")
set(PKG_FILE "${CMAKE_CURRENT_BINARY_DIR}/${MOD_ID}-${MOD_VERSION}-${CMAKE_ANDROID_ARCH_ABI}.levipack")

add_custom_target(levi_package
    COMMAND "${CMAKE_COMMAND}" -E rm -rf "${PKG_ROOT}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PKG_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${MOD_LIBRARY_NAME}>" "${PKG_DIR}/lib${MOD_LIBRARY_NAME}.so"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_BINARY_DIR}/manifest.json" "${PKG_DIR}/manifest.json"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE" "${PKG_DIR}/LICENSE"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.png" "${PKG_DIR}/icon.png"
    COMMAND "${CMAKE_COMMAND}" -E chdir "${PKG_ROOT}" "${CMAKE_COMMAND}" -E tar "cf" "${PKG_FILE}" --format=zip "${MOD_ID}"
    DEPENDS ${MOD_LIBRARY_NAME}
    COMMENT "Packaging ${MOD_ID}.levipack"
    VERBATIM
)
