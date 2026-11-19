# MultiTool - CMake build (alternative to the Visual Studio solution and the Makefile)
#   MSVC :  cmake -S . -B build -G "Visual Studio 17 2022" -A x64  &&  cmake --build build --config Release
#   MinGW:  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release  &&  cmake --build build
#   Cross (Linux -> Windows):  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake_minimum_required(VERSION 3.20)
project(MultiTool VERSION 3.0.0 LANGUAGES C RC)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

file(GLOB_RECURSE MT_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c")

add_executable(multi-tool WIN32 ${MT_SOURCES} res/multitool.rc)
set_target_properties(multi-tool PROPERTIES OUTPUT_NAME "multi-tool")

target_include_directories(multi-tool PRIVATE src res)
target_compile_definitions(multi-tool PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS
    _WINSOCK_DEPRECATED_NO_WARNINGS _WIN32_WINNT=0x0A00 WINVER=0x0A00)

if(MSVC)
    target_compile_options(multi-tool PRIVATE /utf-8 /W3 /wd4996 /wd4189 /wd4100 /wd4244 /wd4267 /wd4018 /wd4146)
    # static CRT: no Visual C++ Redistributable needed on the target PC
    set_property(TARGET multi-tool PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    # The manifest is merged and embedded once by CMake's manifest tool; res/multitool.rc only embeds it for MinGW.
    target_sources(multi-tool PRIVATE res/multitool.manifest)
    # import libraries are pulled in by '#pragma comment(lib, ...)' in src/app/main.c
else()
    target_compile_options(multi-tool PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-cast-function-type -Wno-missing-field-initializers)
    target_link_options(multi-tool PRIVATE -mwindows -static-libgcc)
    target_link_libraries(multi-tool PRIVATE
        gdi32 user32 kernel32 advapi32 shell32 ole32 oleaut32 uuid comdlg32 comctl32 uxtheme dwmapi
        ws2_32 iphlpapi winhttp netapi32 setupapi cfgmgr32 bcrypt wintrust crypt32 psapi powrprof imm32 msimg32)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(multi-tool PRIVATE -s)
    endif()
endif()

install(TARGETS multi-tool RUNTIME DESTINATION .)
install(FILES LICENSE README.md DESTINATION .)
