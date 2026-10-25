cmake_minimum_required(VERSION 3.25)

project(
	MainMenuToggles
	VERSION 1.0.0
	DESCRIPTION "Choose which parts of the Skyrim main menu are visible"
	LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(FATAL_ERROR "In-source builds are not allowed.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # read by clang-tidy (build.ps1 -Lint)
option(MMT_ANALYZE "Compile the plugin with MSVC Code Analysis (build.ps1 -Lint)" OFF)

# SE (1.5.x) + AE (1.6.x / 1.7.x) in one DLL via Address Library. VR is not targeted.
set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(extern/CommonLibSSE-NG EXCLUDE_FROM_ALL SYSTEM)  # SYSTEM keeps its headers out of warnings and analysis
include(extern/CommonLibSSE-NG/cmake/CommonLibSSE.cmake)

find_path(SIMPLEINI_INCLUDE_DIRS "SimpleIni.h" REQUIRED)

set(SOURCES
	src/PCH.h
	src/main.cpp
	src/Settings.h
	src/Settings.cpp
	src/SceneHider.h
	src/SceneHider.cpp
	src/UIHider.h
	src/UIHider.cpp
	src/Controller.h
	src/Controller.cpp
	src/LogoSwap.h
	src/LogoSwap.cpp
)

add_commonlibsse_plugin(
	${PROJECT_NAME}
	AUTHOR "hankinsrim"
	USE_ADDRESS_LIBRARY
	SOURCES ${SOURCES}
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${SIMPLEINI_INCLUDE_DIRS})
target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /Zc:preprocessor /utf-8)
target_link_options(${PROJECT_NAME} PRIVATE /DEBUG:FULL)
if(MMT_ANALYZE)
	target_compile_options(${PROJECT_NAME} PRIVATE /analyze /analyze:external-)
endif()

# Mod-manager-ready layout: dist/SKSE/Plugins/{MainMenuToggles.dll,.pdb,.ini}
set(DIST_DIR "${PROJECT_SOURCE_DIR}/dist/SKSE/Plugins")
add_custom_command(
	TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${DIST_DIR}"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${DIST_DIR}"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_SOURCE_DIR}/data/SKSE/Plugins/MainMenuToggles.ini" "${DIST_DIR}"
	VERBATIM
)
