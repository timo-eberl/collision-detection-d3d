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

# Agility SDK works on all Windows compilers (MSVC, MinGW)
if(WIN32)
	FetchContent_Declare(
		AgilitySDK
		URL https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.614.1
	)
	FetchContent_MakeAvailable(AgilitySDK)
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

# Compile shader (broad_phase_grid_a builder)
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/grid_a_load_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/grid_a_permute_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/grid_a_count_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/grid_a_assign_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/grid_a_bounds_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/grid_a_clear_shader.h
	COMMAND ${DXC_EXECUTABLE}
		-E cs_load_key_value -T cs_6_8 -O3 -Vn grid_a_load_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/grid_a_load_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/broad_phase_grid_a.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_permute_aabbs -T cs_6_8 -O3 -Vn grid_a_permute_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/grid_a_permute_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/broad_phase_grid_a.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_count_cells -T cs_6_8 -O3 -Vn grid_a_count_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/grid_a_count_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/broad_phase_grid_a.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_assign -T cs_6_8 -O3 -Vn grid_a_assign_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/grid_a_assign_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/broad_phase_grid_a.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_find_boundaries -T cs_6_8 -O3 -Vn grid_a_bounds_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/grid_a_bounds_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/broad_phase_grid_a.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_clear_cells -T cs_6_8 -O3 -Vn grid_a_clear_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/grid_a_clear_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/broad_phase_grid_a.hlsl
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/broad_phase_grid_a.hlsl
			${CMAKE_CURRENT_SOURCE_DIR}/src/grid_math.hlsli
)

# Compile shader (algo_simple_naive)
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/algo_simple_naive_aabb_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/algo_simple_naive_broad_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/algo_simple_naive_narrow_shader.h
	COMMAND ${DXC_EXECUTABLE}
		-E cs_aabb_prep -T cs_6_8 -O3
		-Vn algo_simple_naive_aabb_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_simple_naive_aabb_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_simple_naive.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_broad_phase -T cs_6_8 -O3
		-Vn algo_simple_naive_broad_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_simple_naive_broad_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_simple_naive.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_narrow_phase -T cs_6_8 -O3
		-Vn algo_simple_naive_narrow_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_simple_naive_narrow_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_simple_naive.hlsl
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/algo_simple_naive.hlsl
			${CMAKE_CURRENT_SOURCE_DIR}/src/collision_math.hlsli
)

# Compile shader (algo_simple_binned)
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/algo_simple_binned_aabb_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/algo_simple_binned_broad_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/algo_simple_binned_narrow_shader.h
	COMMAND ${DXC_EXECUTABLE}
		-E cs_aabb_prep -T cs_6_8 -O3
		-Vn algo_simple_binned_aabb_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_simple_binned_aabb_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_simple_binned.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_broad_phase -T cs_6_8 -O3
		-Vn algo_simple_binned_broad_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_simple_binned_broad_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_simple_binned.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_narrow_phase -T cs_6_8 -O3
		-Vn algo_simple_binned_narrow_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_simple_binned_narrow_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_simple_binned.hlsl
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/algo_simple_binned.hlsl
			${CMAKE_CURRENT_SOURCE_DIR}/src/collision_math.hlsli
)

# Compile shader (algo_execute_indirect)
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/algo_execute_indirect_aabb_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/algo_execute_indirect_broad_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/algo_execute_indirect_narrow_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/algo_execute_indirect_prep_shader.h
	COMMAND ${DXC_EXECUTABLE}
		-E cs_aabb_prep -T cs_6_8 -O3
		-Vn algo_execute_indirect_aabb_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_execute_indirect_aabb_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_execute_indirect.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_broad_phase -T cs_6_8 -O3
		-Vn algo_execute_indirect_broad_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_execute_indirect_broad_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_execute_indirect.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_narrow_phase -T cs_6_8 -O3
		-Vn algo_execute_indirect_narrow_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_execute_indirect_narrow_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_execute_indirect.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_dispatch_prep -T cs_6_8 -O3
		-Vn algo_execute_indirect_prep_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_execute_indirect_prep_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_execute_indirect.hlsl
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/algo_execute_indirect.hlsl
			${CMAKE_CURRENT_SOURCE_DIR}/src/collision_math.hlsli
)

# Compile shader (algo_work_graphs)
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/algo_work_graphs_lib_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/algo_work_graphs_init_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/algo_work_graphs_aabb_shader.h
		   ${CMAKE_CURRENT_BINARY_DIR}/algo_work_graphs_broad_shader.h
	COMMAND ${DXC_EXECUTABLE}
		-T lib_6_8 -O3
		-Vn algo_work_graphs_lib_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_work_graphs_lib_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_work_graphs.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_init_graph -T cs_6_8 -O3
		-Vn algo_work_graphs_init_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_work_graphs_init_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_work_graphs.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_aabb_prep -T cs_6_8 -O3
		-Vn algo_work_graphs_aabb_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_work_graphs_aabb_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_work_graphs.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E cs_broad_phase -T cs_6_8 -O3
		-Vn algo_work_graphs_broad_shader
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/algo_work_graphs_broad_shader.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/algo_work_graphs.hlsl
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/algo_work_graphs.hlsl
			${CMAKE_CURRENT_SOURCE_DIR}/src/collision_math.hlsli
)

# Compile shader (prefix_sum)
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/prefix_sum_init.h
		   ${CMAKE_CURRENT_BINARY_DIR}/prefix_sum_exc.h
		   ${CMAKE_CURRENT_BINARY_DIR}/prefix_sum_inc_max.h
	COMMAND ${DXC_EXECUTABLE}
		-E InitChainedScan -T cs_6_8 -O3 -Wno-conversion
		-Vn prefix_sum_init
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/prefix_sum_init.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/prefix_sum/prefix_sum.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E ChainedScanDecoupledLookback -T cs_6_8 -O3 -Wno-conversion
		-Vn prefix_sum_exc
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/prefix_sum_exc.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/prefix_sum/prefix_sum.hlsl
	COMMAND ${DXC_EXECUTABLE}
		-E ChainedScanDecoupledLookback -T cs_6_8 -O3 -Wno-conversion -DOP_MAX
		-Vn prefix_sum_inc_max
		-Fh ${CMAKE_CURRENT_BINARY_DIR}/prefix_sum_inc_max.h
		${CMAKE_CURRENT_SOURCE_DIR}/src/prefix_sum/prefix_sum.hlsl
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/prefix_sum/prefix_sum.hlsl
)

# Compile shader (radix_sort)
set(RS_BASE_FLAGS -DKEY_UINT -DPAYLOAD_UINT -DSORT_PAIRS -DSHOULD_ASCEND -DENABLE_16_BIT)
set(RS_DXC_CMD ${DXC_EXECUTABLE} -T cs_6_8 -O3 -enable-16bit-types -Wno-conversion)

# Radix Sort: NVIDIA Profile
set(RS_NV_FLAGS ${RS_BASE_FLAGS} -DKEYS_PER_THREAD_15 -DPART_SIZE_7680)
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gpusort_nv_init.h
		   ${CMAKE_CURRENT_BINARY_DIR}/gpusort_nv_upsweep.h
		   ${CMAKE_CURRENT_BINARY_DIR}/gpusort_nv_scan.h
		   ${CMAKE_CURRENT_BINARY_DIR}/gpusort_nv_downsweep.h
	COMMAND ${RS_DXC_CMD} ${RS_NV_FLAGS} -E InitDeviceRadixSort -Vn gpusort_nv_init -Fh ${CMAKE_CURRENT_BINARY_DIR}/gpusort_nv_init.h ${CMAKE_CURRENT_SOURCE_DIR}/src/radix_sort/shaders/DeviceRadixSort.hlsl
	COMMAND ${RS_DXC_CMD} ${RS_NV_FLAGS} -E Upsweep -Vn gpusort_nv_upsweep -Fh ${CMAKE_CURRENT_BINARY_DIR}/gpusort_nv_upsweep.h ${CMAKE_CURRENT_SOURCE_DIR}/src/radix_sort/shaders/DeviceRadixSort.hlsl
	COMMAND ${RS_DXC_CMD} ${RS_NV_FLAGS} -E Scan -Vn gpusort_nv_scan -Fh ${CMAKE_CURRENT_BINARY_DIR}/gpusort_nv_scan.h ${CMAKE_CURRENT_SOURCE_DIR}/src/radix_sort/shaders/DeviceRadixSort.hlsl
	COMMAND ${RS_DXC_CMD} ${RS_NV_FLAGS} -E Downsweep -Vn gpusort_nv_downsweep -Fh ${CMAKE_CURRENT_BINARY_DIR}/gpusort_nv_downsweep.h ${CMAKE_CURRENT_SOURCE_DIR}/src/radix_sort/shaders/DeviceRadixSort.hlsl
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/radix_sort/shaders/DeviceRadixSort.hlsl
)

# Radix Sort: AMD Profile
set(RS_AMD_FLAGS ${RS_BASE_FLAGS} -DLOCK_TO_W32 -DKEYS_PER_THREAD_5 -DPART_SIZE_2560 -DD_TOTAL_SMEM_4096)
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gpusort_amd_init.h
		   ${CMAKE_CURRENT_BINARY_DIR}/gpusort_amd_upsweep.h
		   ${CMAKE_CURRENT_BINARY_DIR}/gpusort_amd_scan.h
		   ${CMAKE_CURRENT_BINARY_DIR}/gpusort_amd_downsweep.h
	COMMAND ${RS_DXC_CMD} ${RS_AMD_FLAGS} -E InitDeviceRadixSort -Vn gpusort_amd_init -Fh ${CMAKE_CURRENT_BINARY_DIR}/gpusort_amd_init.h ${CMAKE_CURRENT_SOURCE_DIR}/src/radix_sort/shaders/DeviceRadixSort.hlsl
	COMMAND ${RS_DXC_CMD} ${RS_AMD_FLAGS} -E Upsweep -Vn gpusort_amd_upsweep -Fh ${CMAKE_CURRENT_BINARY_DIR}/gpusort_amd_upsweep.h ${CMAKE_CURRENT_SOURCE_DIR}/src/radix_sort/shaders/DeviceRadixSort.hlsl
	COMMAND ${RS_DXC_CMD} ${RS_AMD_FLAGS} -E Scan -Vn gpusort_amd_scan -Fh ${CMAKE_CURRENT_BINARY_DIR}/gpusort_amd_scan.h ${CMAKE_CURRENT_SOURCE_DIR}/src/radix_sort/shaders/DeviceRadixSort.hlsl
	COMMAND ${RS_DXC_CMD} ${RS_AMD_FLAGS} -E Downsweep -Vn gpusort_amd_downsweep -Fh ${CMAKE_CURRENT_BINARY_DIR}/gpusort_amd_downsweep.h ${CMAKE_CURRENT_SOURCE_DIR}/src/radix_sort/shaders/DeviceRadixSort.hlsl
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/radix_sort/shaders/DeviceRadixSort.hlsl
)

# Library
add_library(${PROJECT_NAME}
	src/shared_state.cpp
	src/broad_phase_grid_a.cpp
	src/algo_simple_naive.cpp
	src/algo_simple_binned.cpp
	src/algo_execute_indirect.cpp
	src/algo_work_graphs.cpp
	src/prefix_sum/prefix_sum.cpp
	src/radix_sort/radix_sort.cpp
	${CMAKE_CURRENT_BINARY_DIR}/grid_a_load_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/grid_a_permute_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/grid_a_count_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/grid_a_assign_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/grid_a_bounds_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/grid_a_clear_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/algo_simple_naive_aabb_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/algo_simple_naive_broad_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/algo_simple_naive_narrow_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/algo_simple_binned_aabb_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/algo_simple_binned_broad_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/algo_simple_binned_narrow_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/algo_execute_indirect_aabb_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/algo_execute_indirect_broad_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/algo_execute_indirect_narrow_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/algo_execute_indirect_prep_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/algo_work_graphs_lib_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/algo_work_graphs_init_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/algo_work_graphs_aabb_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/algo_work_graphs_broad_shader.h
	${CMAKE_CURRENT_BINARY_DIR}/prefix_sum_init.h
	${CMAKE_CURRENT_BINARY_DIR}/prefix_sum_exc.h
	${CMAKE_CURRENT_BINARY_DIR}/prefix_sum_inc_max.h
	${CMAKE_CURRENT_BINARY_DIR}/gpusort_nv_init.h
	${CMAKE_CURRENT_BINARY_DIR}/gpusort_nv_upsweep.h
	${CMAKE_CURRENT_BINARY_DIR}/gpusort_nv_scan.h
	${CMAKE_CURRENT_BINARY_DIR}/gpusort_nv_downsweep.h
	${CMAKE_CURRENT_BINARY_DIR}/gpusort_amd_init.h
	${CMAKE_CURRENT_BINARY_DIR}/gpusort_amd_upsweep.h
	${CMAKE_CURRENT_BINARY_DIR}/gpusort_amd_scan.h
	${CMAKE_CURRENT_BINARY_DIR}/gpusort_amd_downsweep.h
)

target_include_directories(${PROJECT_NAME}
	PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include # public API
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/prefix_sum
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/radix_sort
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

if(WIN32)
	# Ensure the Agility SDK dlls are copied to a D3D12 subfolder next to the executable
	add_custom_command(TARGET ${PROJECT_NAME}_app POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory
		$<TARGET_FILE_DIR:${PROJECT_NAME}_app>/D3D12
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${agilitysdk_SOURCE_DIR}/build/native/bin/x64/D3D12Core.dll"
		$<TARGET_FILE_DIR:${PROJECT_NAME}_app>/D3D12/D3D12Core.dll
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${agilitysdk_SOURCE_DIR}/build/native/bin/x64/d3d12SDKLayers.dll"
		$<TARGET_FILE_DIR:${PROJECT_NAME}_app>/D3D12/d3d12SDKLayers.dll
	)
endif()
