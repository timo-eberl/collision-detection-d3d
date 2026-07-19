cmake_minimum_required(VERSION 3.15)
project(collision_dx LANGUAGES C CXX)

option(CDDX_ENABLE_PROFILER "Print profiling messages to stderr" ON)

if(POLICY CMP0135)
	cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)
FetchContent_Declare(
	DirectX-Headers
	GIT_REPOSITORY https://github.com/microsoft/DirectX-Headers.git
	GIT_TAG        v1.619.4
)
FetchContent_MakeAvailable(DirectX-Headers)

if(MSVC)
	FetchContent_Declare(
		WinPixEventRuntime
		URL https://www.nuget.org/api/v2/package/WinPixEventRuntime/1.0.240308001
	)
	FetchContent_MakeAvailable(WinPixEventRuntime)
endif()

find_program(DXC_EXECUTABLE dxc)

if(NOT DXC_EXECUTABLE)
	message(STATUS "dxc not found. Downloading prebuilt Microsoft DirectXShaderCompiler...")
	if(WIN32)
		set(DXC_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602.24/dxc_2026_05_27.zip")
		set(DXC_HASH "SHA256=cf658aacf070d3045e31b8f1f8a696c2945f37c1095019481ef7c513368db3b4")
		set(DXC_BIN_RELATIVE "bin/x64/dxc.exe")
	else()
		set(DXC_URL "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2602.24/linux_dxc_2026_05_26.x86_64.tar.gz")
		set(DXC_HASH "SHA256=928b3e9986d11dc4279050e02340950c29bcbd1e5efb9d3ded9669dade37639d")
		set(DXC_BIN_RELATIVE "bin/dxc")
	endif()

	FetchContent_Declare(dxc_prebuilt URL ${DXC_URL} URL_HASH ${DXC_HASH})
	FetchContent_GetProperties(dxc_prebuilt)
	if(NOT dxc_prebuilt_POPULATED)
		FetchContent_MakeAvailable(dxc_prebuilt)
	endif()
	set(DXC_EXECUTABLE "${dxc_prebuilt_SOURCE_DIR}/${DXC_BIN_RELATIVE}")

	if(UNIX)
		execute_process(COMMAND chmod +x "${DXC_EXECUTABLE}")
	endif()
endif()

message(STATUS "Using DXC: ${DXC_EXECUTABLE}")

# Compile shader
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/algo_simple_shader.h
	COMMAND ${DXC_EXECUTABLE}
		-E main
		-T cs_6_0
		-O3
		-Vn algo_simple_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_simple_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_simple.hlsl
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/algo_simple.hlsl
)

# Library
add_library(${PROJECT_NAME}
	src/shared_state.cpp
	src/algo_simple.cpp
	${CMAKE_CURRENT_BINARY_DIR}/algo_simple_shader.h
)

target_include_directories(${PROJECT_NAME}
	PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include # public API
	PRIVATE ${CMAKE_CURRENT_BINARY_DIR}
)

if(MSVC)
	target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE
		"${winpixeventruntime_SOURCE_DIR}/Include/WinPixEventRuntime"
	)
endif()

# Link D3D/DXCore/PIX to the library
if(UNIX)
	target_link_directories(${PROJECT_NAME} PUBLIC "/usr/lib/wsl/lib")
	find_library(D3D12_LIB d3d12 PATHS "/usr/lib/wsl/lib" REQUIRED)
	find_library(DXCORE_LIB dxcore PATHS "/usr/lib/wsl/lib" REQUIRED)

	target_link_libraries(${PROJECT_NAME} PRIVATE DirectX-Headers DirectX-Guids ${D3D12_LIB} ${DXCORE_LIB})
else()
	if(MSVC)
		target_link_directories(${PROJECT_NAME} PRIVATE "${winpixeventruntime_SOURCE_DIR}/bin/x64")
		target_link_libraries(${PROJECT_NAME} PRIVATE DirectX-Headers DirectX-Guids d3d12 dxcore WinPixEventRuntime)
	else()
		target_link_libraries(${PROJECT_NAME} PRIVATE DirectX-Headers DirectX-Guids d3d12 dxcore)
	endif()
endif()

if(CDDX_ENABLE_PROFILER)
	target_compile_definitions(${PROJECT_NAME} PUBLIC CDDX_ENABLE_PROFILER)
endif()

# Build executable
add_executable(${PROJECT_NAME}_app src/main.cpp)
target_link_libraries(${PROJECT_NAME}_app PRIVATE ${PROJECT_NAME})

if(MSVC)
	# Ensure the PIX dll is copied next to the executable
	add_custom_command(TARGET ${PROJECT_NAME}_app POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${winpixeventruntime_SOURCE_DIR}/bin/x64/WinPixEventRuntime.dll"
		$<TARGET_FILE_DIR:${PROJECT_NAME}_app>
	)
endif()
