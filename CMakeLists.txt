# MealKit QA: views a game's bug reports, crash reports and replays and opens them in the MealKit editor (SDL3 + Dear ImGui, same framework as
# MealKit Audio and MealKit Manager). Doesn't link the engine: reads the files the game writes in its user folder (include/mkqa/Reports.h).
#   build.bat                 (Windows: vcvars64 + Ninja, build\bin\MealKitQA.exe, including tests)
#   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build && ctest --test-dir build
#   cmake --install build --prefix "%LOCALAPPDATA%\MealKit\tools\qa"   (the editor's [Tools] menu finds it there)
cmake_minimum_required(VERSION 3.25)
project(mkqa VERSION 0.1.0 LANGUAGES C CXX)

option(MKQA_BUILD_TOOL "Build the MealKitQA window (SDL3 + Dear ImGui)" ON)

# Tool contract (MealKit-Contract, copied into third_party/mkcontract with that repo's tools/vendor.ps1): files, user folder, manifest rules
set(MKCONTRACT_BUILD_TESTS OFF)
add_subdirectory(third_party/mkcontract)

# Data layer (no window, used by tests)
add_library(mkqa STATIC include/mkqa/Reports.h src/Reports.cpp src/Strings.h src/Strings.cpp)
target_include_directories(mkqa PUBLIC include src)
target_link_libraries(mkqa PUBLIC mkcontract)
target_compile_features(mkqa PUBLIC cxx_std_20)

if(MKQA_BUILD_TOOL)
    # SDL3 (static, without tests/examples)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)

    set(imgui_dir ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
    add_library(mkqa_imgui STATIC
        ${imgui_dir}/imgui.cpp ${imgui_dir}/imgui_draw.cpp ${imgui_dir}/imgui_tables.cpp ${imgui_dir}/imgui_widgets.cpp
        ${imgui_dir}/backends/imgui_impl_sdl3.cpp ${imgui_dir}/backends/imgui_impl_sdlrenderer3.cpp)
    target_include_directories(mkqa_imgui PUBLIC ${imgui_dir} ${imgui_dir}/backends)
    target_link_libraries(mkqa_imgui PUBLIC SDL3::SDL3-static)
    if(MSVC)
        target_compile_options(mkqa_imgui PRIVATE /utf-8)
    endif()

    add_executable(MealKitQA WIN32 tool/main.cpp tool/App.h tool/App.cpp)
    target_link_libraries(MealKitQA PRIVATE mkqa mkqa_imgui SDL3::SDL3-static)
    target_compile_definitions(MealKitQA PRIVATE MKQA_VERSION="${PROJECT_VERSION}")
    set_target_properties(MealKitQA PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    foreach(config Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER ${config} upper)
        set_target_properties(MealKitQA PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${upper} ${CMAKE_BINARY_DIR}/bin)
    endforeach()

    # Manifest next to the program (MealKit tool contract). exe is written without an extension, .exe is appended on Windows
    set(MKQA_VERSION ${PROJECT_VERSION})
    configure_file(tool/mealkit-tool.txt ${CMAKE_BINARY_DIR}/bin/mealkit-tool.txt @ONLY)
    if(WIN32)
        file(READ ${CMAKE_BINARY_DIR}/bin/mealkit-tool.txt manifest)
        string(REPLACE "exe MealKitQA\n" "exe MealKitQA.exe\n" manifest "${manifest}")
        file(WRITE ${CMAKE_BINARY_DIR}/bin/mealkit-tool.txt "${manifest}")
    endif()
    install(TARGETS MealKitQA RUNTIME DESTINATION .)
    install(FILES ${CMAKE_BINARY_DIR}/bin/mealkit-tool.txt DESTINATION .)
endif()

foreach(target mkqa MealKitQA)
    if(NOT TARGET ${target})
        continue()
    endif()
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endforeach()

enable_testing()
add_executable(mkqa_tests tests/QaTests.cpp)
target_link_libraries(mkqa_tests PRIVATE mkqa)
target_compile_definitions(mkqa_tests PRIVATE MKQA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(mkqa_tests PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(mkqa_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
add_test(NAME mkqa_tests COMMAND mkqa_tests)
