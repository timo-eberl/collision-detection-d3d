#include "algo_work_graphs_aabb_shader.h"
#include "algo_work_graphs_broad_shader.h"
#include "algo_work_graphs_init_shader.h"
#include "algo_work_graphs_lib_shader.h"
#include "collision_detection_d3d.h"
#include "dx_profile.h"
#include "shared.h"
#include "broad_phase_grid_a.h"

#ifdef _MSC_VER
	#include <pix3.h>
#else
	#define PIXBeginEvent(...)
	#define PIXEndEvent(...)
	#define PIX_COLOR(r, g, b) 0
#endif

#include <string.h>

struct dx_potential_pair {
	uint32_t a_index;
	uint32_t b_index;
	uint32_t b_type;
	uint32_t pad;
};

typedef struct { float min_x, max_x, min_y, max_y, min_z, max_z; uint32_t shape_type; uint32_t pad; } packed_aabb;

// Matches D3D12_NODE_GPU_INPUT memory layout.
struct dx_node_gpu_input {
	uint32_t entrypoint_index;
	uint32_t num_records;
	uint64_t records_address;
	uint64_t records_stride;
};

struct dx_state_work_graphs {
	dx_state_grid_a* grid_builder;

	ID3D12RootSignature* root_sig;
	ID3D12PipelineState* pso_aabb_prep;
	ID3D12PipelineState* pso_broad_phase;
	ID3D12PipelineState* pso_init_graph;
	ID3D12StateObject* work_graph_state_object;

	D3D12_PROGRAM_IDENTIFIER wg_program_id;
	uint32_t wg_entrypoint_index;
	uint32_t wg_index;

	ID3D12Resource* d_aabb_rigids;
	size_t d_aabb_rigids_size;
	ID3D12Resource* d_aabb_statics;
	size_t d_aabb_statics_size;

	ID3D12Resource* d_potential_pairs;
	size_t d_potential_pairs_size;

	ID3D12Resource* up_gpu_input;
	size_t up_gpu_input_size;
	ID3D12Resource* d_gpu_input;
	size_t d_gpu_input_size;

	ID3D12Resource* d_backing_mem;
	size_t d_backing_mem_size;

	dx_collision_compact* h_cols;
	size_t h_cols_capacity;
};

static void bind_universal_context(ID3D12GraphicsCommandList7* cmd, dx_shared_state* sh,
								   dx_state_work_graphs* state, uint32_t static_count) {
	D3D12_GPU_VIRTUAL_ADDRESS rigids = sh->d_rigids->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS statics = static_count > 0 ? sh->d_statics->GetGPUVirtualAddress() : rigids;
	D3D12_GPU_VIRTUAL_ADDRESS aabb_r = state->d_aabb_rigids->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS aabb_s = static_count > 0 ? state->d_aabb_statics->GetGPUVirtualAddress() : aabb_r;

	cmd->SetComputeRootShaderResourceView(2, rigids); // t0
	cmd->SetComputeRootShaderResourceView(3, statics); // t1
	cmd->SetComputeRootShaderResourceView(4, sh->d_shapes->GetGPUVirtualAddress()); // t2
	cmd->SetComputeRootShaderResourceView(5, aabb_r); // t3
	cmd->SetComputeRootShaderResourceView(6, aabb_s); // t4

	// Grid outputs (t5 - t9)
	cmd->SetComputeRootShaderResourceView(7, dx_grid_a_get_sorted_keys(state->grid_builder)); 
	cmd->SetComputeRootShaderResourceView(8, dx_grid_a_get_sorted_aabbs(state->grid_builder));
	cmd->SetComputeRootShaderResourceView(9, dx_grid_a_get_sorted_indices(state->grid_builder));
	cmd->SetComputeRootShaderResourceView(10, dx_grid_a_get_sorted_vals(state->grid_builder));
	cmd->SetComputeRootShaderResourceView(11, dx_grid_a_get_cell_ends(state->grid_builder));

	cmd->SetComputeRootUnorderedAccessView(12, 0); // u0 (AABB UAV) set per-dispatch
	cmd->SetComputeRootUnorderedAccessView(13, state->d_potential_pairs->GetGPUVirtualAddress()); // u1
	cmd->SetComputeRootUnorderedAccessView(14, state->d_gpu_input->GetGPUVirtualAddress()); // u2
	cmd->SetComputeRootUnorderedAccessView(15, sh->d_collisions->GetGPUVirtualAddress()); // u3
	cmd->SetComputeRootUnorderedAccessView(16, sh->d_col_count->GetGPUVirtualAddress()); // u4
}

extern "C" dx_state_work_graphs* dx_state_work_graphs_create(dx_shared_state* sh) {
	D3D12_FEATURE_DATA_D3D12_OPTIONS21 options21 = {};
	HRESULT hr = sh->device->CheckFeatureSupport(
		D3D12_FEATURE_D3D12_OPTIONS21, &options21, sizeof(options21));
		
	if (FAILED(hr) || options21.WorkGraphsTier == D3D12_WORK_GRAPHS_TIER_NOT_SUPPORTED) {
		fprintf(stderr, "[dx12] Work Graphs are not supported on this device/driver.\n");
		return nullptr;
	}

	dx_state_work_graphs* s = (dx_state_work_graphs*)calloc(1, sizeof(dx_state_work_graphs));
	
	s->grid_builder = dx_grid_a_create(sh->device, sh->is_amd);

	D3D12_ROOT_PARAMETER root_params[17] = {};
	
	// b0: Constants
	root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	root_params[0].Constants.ShaderRegister = 0;
	root_params[0].Constants.RegisterSpace = 0;
	root_params[0].Constants.Num32BitValues = 5;
	root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// b1: GridConstants
	root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	root_params[1].Constants.ShaderRegister = 1;
	root_params[1].Constants.RegisterSpace = 0;
	root_params[1].Constants.Num32BitValues = 11;
	root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// SRVs: t0 to t9
	for (int i = 0; i < 10; ++i) {
		root_params[2 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		root_params[2 + i].Descriptor.ShaderRegister = i;
		root_params[2 + i].Descriptor.RegisterSpace = 0;
		root_params[2 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	// UAVs: u0 to u4
	for (int i = 0; i < 5; ++i) {
		root_params[12 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		root_params[12 + i].Descriptor.ShaderRegister = i;
		root_params[12 + i].Descriptor.RegisterSpace = 0;
		root_params[12 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	D3D12_ROOT_SIGNATURE_DESC rs_desc = {};
	rs_desc.NumParameters = 17;
	rs_desc.pParameters = root_params;

	ID3DBlob* signature = nullptr;
	ID3DBlob* error = nullptr;
	hr = D3D12SerializeRootSignature(
		&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

	if (FAILED(hr)) {
		if (error) {
			fprintf(stderr, "[dx12] Root Signature Error: %s\n", (char*)error->GetBufferPointer());
			error->Release();
		}
		free(s);
		return nullptr;
	}

	DX_CHECK(sh->device->CreateRootSignature(
		0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&s->root_sig)));
	s->root_sig->SetName(L"WorkGraphs_Root_Sig");
	signature->Release();

	D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
	pso_desc.pRootSignature = s->root_sig;

	pso_desc.CS.pShaderBytecode = algo_work_graphs_aabb_shader;
	pso_desc.CS.BytecodeLength = sizeof(algo_work_graphs_aabb_shader);
	DX_CHECK(sh->device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&s->pso_aabb_prep)));

	pso_desc.CS.pShaderBytecode = algo_work_graphs_broad_shader;
	pso_desc.CS.BytecodeLength = sizeof(algo_work_graphs_broad_shader);
	DX_CHECK(sh->device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&s->pso_broad_phase)));

	pso_desc.CS.pShaderBytecode = algo_work_graphs_init_shader;
	pso_desc.CS.BytecodeLength = sizeof(algo_work_graphs_init_shader);
	DX_CHECK(sh->device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&s->pso_init_graph)));

	// Create Work Graph State Object
	D3D12_GLOBAL_ROOT_SIGNATURE global_rs_desc = { s->root_sig };
	D3D12_STATE_SUBOBJECT subobjects[3] = {};

	subobjects[0].Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	subobjects[0].pDesc = &global_rs_desc;

	D3D12_DXIL_LIBRARY_DESC dxil_lib_desc = {};
	dxil_lib_desc.DXILLibrary.pShaderBytecode = algo_work_graphs_lib_shader;
	dxil_lib_desc.DXILLibrary.BytecodeLength = sizeof(algo_work_graphs_lib_shader);
	
	const wchar_t* export_names[] = {
		L"RoutePairs",
		L"Narrow_Sph_Sph",
		L"Narrow_Sph_Cap",
		L"Narrow_Sph_Box",
		L"Narrow_Cap_Cap",
		L"Narrow_Cap_Box",
		L"Narrow_Box_Box"
	};
	D3D12_EXPORT_DESC exports[7] = {};
	for (int i = 0; i < 7; ++i) {
		exports[i].Name = export_names[i];
		exports[i].Flags = D3D12_EXPORT_FLAG_NONE;
	}
	dxil_lib_desc.NumExports = 7;
	dxil_lib_desc.pExports = exports;

	subobjects[1].Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	subobjects[1].pDesc = &dxil_lib_desc;

	D3D12_NODE_ID entry_node = { L"RoutePairs", 0 };
	D3D12_WORK_GRAPH_DESC wg_desc = {};
	wg_desc.ProgramName = L"CollisionGraph";
	wg_desc.NumEntrypoints = 1;
	wg_desc.pEntrypoints = &entry_node;
	wg_desc.Flags = D3D12_WORK_GRAPH_FLAG_NONE;

	subobjects[2].Type = D3D12_STATE_SUBOBJECT_TYPE_WORK_GRAPH;
	subobjects[2].pDesc = &wg_desc;

	D3D12_STATE_OBJECT_DESC so_desc = {};
	so_desc.Type = D3D12_STATE_OBJECT_TYPE_EXECUTABLE;
	so_desc.NumSubobjects = 3;
	so_desc.pSubobjects = subobjects;

	hr = sh->device->CreateStateObject(&so_desc, IID_PPV_ARGS(&s->work_graph_state_object));
	if (FAILED(hr) || !s->work_graph_state_object) {
		fprintf(stderr, "[dx12] Failed to create Work Graph State Object.\n");
		if (s->pso_aabb_prep) s->pso_aabb_prep->Release();
		if (s->pso_broad_phase) s->pso_broad_phase->Release();
		if (s->pso_init_graph) s->pso_init_graph->Release();
		if (s->root_sig) s->root_sig->Release();
		free(s);
		return nullptr;
	}

	ID3D12StateObjectProperties1* so_props = nullptr;
	s->work_graph_state_object->QueryInterface(IID_PPV_ARGS(&so_props));
#ifdef _MSC_VER
	// MSVC uses return-by-value
	s->wg_program_id = so_props->GetProgramIdentifier(L"CollisionGraph");
#else
	// GCC (DirectX-Headers) requires an explicit output pointer
	so_props->GetProgramIdentifier(&s->wg_program_id, L"CollisionGraph");
#endif

	ID3D12WorkGraphProperties* wg_props = nullptr;
	s->work_graph_state_object->QueryInterface(IID_PPV_ARGS(&wg_props));
	s->wg_index = wg_props->GetWorkGraphIndex(L"CollisionGraph");
	s->wg_entrypoint_index = wg_props->GetEntrypointIndex(s->wg_index, entry_node);

	wg_props->Release();
	so_props->Release();

	return s;
}

extern "C" void dx_state_work_graphs_destroy(dx_state_work_graphs* s) {
	if (!s) return;
	dx_grid_a_destroy(s->grid_builder);
	if (s->root_sig) s->root_sig->Release();
	if (s->pso_aabb_prep) s->pso_aabb_prep->Release();
	if (s->pso_broad_phase) s->pso_broad_phase->Release();
	if (s->pso_init_graph) s->pso_init_graph->Release();
	if (s->work_graph_state_object) s->work_graph_state_object->Release();
	if (s->d_aabb_rigids) s->d_aabb_rigids->Release();
	if (s->d_aabb_statics) s->d_aabb_statics->Release();
	if (s->d_potential_pairs) s->d_potential_pairs->Release();
	if (s->up_gpu_input) s->up_gpu_input->Release();
	if (s->d_gpu_input) s->d_gpu_input->Release();
	if (s->d_backing_mem) s->d_backing_mem->Release();
	if (s->h_cols) free(s->h_cols);
	free(s);
}

extern "C" dx_collision_compact*
dx_run_work_graphs(dx_shared_state* sh, dx_state_work_graphs* state, const dx_grid_config* config,
				   const dx_entity* rigids, uint32_t rigid_count, const dx_entity* statics, 
				   uint32_t static_count, const dx_shape* shapes, uint32_t shape_count, 
				   bool statics_changed, uint32_t* out_count) {
	*out_count = 0;
	if (rigid_count == 0) return nullptr;

	size_t cols_needed = sh->d_collisions_size;
	if (cols_needed < 16 * ((size_t)rigid_count + static_count)) {
		cols_needed = 16 * ((size_t)rigid_count + static_count);
	}
	if (cols_needed < 1024) cols_needed = 1024;

	uint32_t count = 0;
	const uint32_t block_size = 256;
	uint32_t grid_size = (rigid_count + block_size - 1) / block_size;
	dx_profile prof = {0};

	shared_ensure_buffers(sh, rigid_count, static_count, shape_count, cols_needed);
	shared_write_inputs(sh->up_rigids, rigids, rigid_count, sh->up_statics, statics,
						static_count, statics_changed, sh->up_shapes, shapes, shape_count);

	uint32_t kernel_max = (sh->d_collisions_size > (size_t)UINT32_MAX)
							  ? UINT32_MAX : (uint32_t)sh->d_collisions_size;

	ensure_dx_buffer(sh->device, &state->d_aabb_rigids, &state->d_aabb_rigids_size, rigid_count,
					 sizeof(packed_aabb), D3D12_HEAP_TYPE_DEFAULT,
					 D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	if (static_count > 0) {
		ensure_dx_buffer(sh->device, &state->d_aabb_statics, &state->d_aabb_statics_size,
						 static_count, sizeof(packed_aabb), D3D12_HEAP_TYPE_DEFAULT,
						 D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	}

	ensure_dx_buffer(sh->device, &state->d_potential_pairs, &state->d_potential_pairs_size,
					 kernel_max, sizeof(dx_potential_pair), D3D12_HEAP_TYPE_DEFAULT,
					 D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);

	ensure_dx_buffer(sh->device, &state->d_gpu_input, &state->d_gpu_input_size, 1,
					 sizeof(dx_node_gpu_input), D3D12_HEAP_TYPE_DEFAULT,
					 D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	ensure_dx_buffer(sh->device, &state->up_gpu_input, &state->up_gpu_input_size, 1,
					 sizeof(dx_node_gpu_input), D3D12_HEAP_TYPE_UPLOAD,
					 D3D12_RESOURCE_FLAG_NONE, 1.0f);

	// Retrieve driver memory requirements
	ID3D12WorkGraphProperties* wg_props = nullptr;
	state->work_graph_state_object->QueryInterface(IID_PPV_ARGS(&wg_props));
	D3D12_WORK_GRAPH_MEMORY_REQUIREMENTS mem_req = {};
	wg_props->GetWorkGraphMemoryRequirements(state->wg_index, &mem_req);
	wg_props->Release();

	bool init_backing_mem = false;
	if (state->d_backing_mem_size < mem_req.MaxSizeInBytes) {
		init_backing_mem = true;
	}
	if (mem_req.MaxSizeInBytes > 0) {
		ensure_dx_buffer(sh->device, &state->d_backing_mem, &state->d_backing_mem_size,
						 mem_req.MaxSizeInBytes, 1, D3D12_HEAP_TYPE_DEFAULT,
						 D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	}

	dx_profile_begin(&prof, sh);
	PIXBeginEvent(sh->cmd_list, PIX_COLOR(255, 0, 0), "Phase: Upload Memory");

	sh->cmd_list->CopyBufferRegion(sh->d_rigids, 0, sh->up_rigids, 0,
								   rigid_count * sizeof(dx_entity));
	sh->cmd_list->CopyBufferRegion(sh->d_shapes, 0, sh->up_shapes, 0,
								   shape_count * sizeof(dx_shape));
	if (statics_changed && static_count > 0) {
		sh->cmd_list->CopyBufferRegion(sh->d_statics, 0, sh->up_statics, 0,
									   static_count * sizeof(dx_entity));
	}
	// Zero out collision count output
	sh->cmd_list->CopyBufferRegion(sh->d_col_count, 0, sh->up_zero, 0, sizeof(uint32_t));

	// Pre-fill the GPU Input struct header
	dx_node_gpu_input input_header = {};
	input_header.entrypoint_index = state->wg_entrypoint_index;
	input_header.num_records = 0; // The broad phase will populate this atomically
	input_header.records_address = state->d_potential_pairs->GetGPUVirtualAddress();
	input_header.records_stride = sizeof(dx_potential_pair);
	
	void* mapped = nullptr;
	state->up_gpu_input->Map(0, nullptr, &mapped);
	memcpy(mapped, &input_header, sizeof(dx_node_gpu_input));
	state->up_gpu_input->Unmap(0, nullptr);

	sh->cmd_list->CopyBufferRegion(state->d_gpu_input, 0, state->up_gpu_input, 0,
								   sizeof(dx_node_gpu_input));

	D3D12_GLOBAL_BARRIER gb_upload = {};
	gb_upload.SyncBefore = D3D12_BARRIER_SYNC_COPY;
	gb_upload.SyncAfter = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb_upload.AccessBefore = D3D12_BARRIER_ACCESS_COPY_DEST;
	gb_upload.AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE |
							D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;

	D3D12_BARRIER_GROUP bg_upload = {};
	bg_upload.Type = D3D12_BARRIER_TYPE_GLOBAL;
	bg_upload.NumBarriers = 1;
	bg_upload.pGlobalBarriers = &gb_upload;
	sh->cmd_list->Barrier(1, &bg_upload);

	PIXEndEvent(sh->cmd_list);
	dx_profile_step(&prof, sh, "upload");

	// --- AABB Prep Phase ---
	sh->cmd_list->SetComputeRootSignature(state->root_sig);
	sh->cmd_list->SetPipelineState(state->pso_aabb_prep);

	bind_universal_context(sh->cmd_list, sh, state, static_count);

	PIXBeginEvent(sh->cmd_list, PIX_COLOR(255, 255, 0), "Phase: AABB Prep");

	uint32_t constants[5] = { rigid_count, rigid_count, static_count, kernel_max, 0 };
	sh->cmd_list->SetComputeRoot32BitConstants(0, 5, constants, 0);

	// Prep Rigids
	sh->cmd_list->SetComputeRootUnorderedAccessView(12, state->d_aabb_rigids->GetGPUVirtualAddress()); // u0
	sh->cmd_list->Dispatch(grid_size, 1, 1);

	// Prep Statics
	if (statics_changed && static_count > 0) {
		constants[0] = static_count;
		sh->cmd_list->SetComputeRoot32BitConstants(0, 5, constants, 0);
		sh->cmd_list->SetComputeRootShaderResourceView(2, sh->d_statics->GetGPUVirtualAddress()); // t0
		sh->cmd_list->SetComputeRootUnorderedAccessView(12, state->d_aabb_statics->GetGPUVirtualAddress()); // u0
		uint32_t static_grid = (static_count + block_size - 1) / block_size;
		sh->cmd_list->Dispatch(static_grid, 1, 1);
	}

	PIXEndEvent(sh->cmd_list);
	dx_profile_step(&prof, sh, "aabb_prep");

	D3D12_GLOBAL_BARRIER gb_prep = {};
	gb_prep.SyncBefore = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb_prep.SyncAfter = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb_prep.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	gb_prep.AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE |
						  D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;

	D3D12_BARRIER_GROUP bg_prep = {};
	bg_prep.Type = D3D12_BARRIER_TYPE_GLOBAL;
	bg_prep.NumBarriers = 1;
	bg_prep.pGlobalBarriers = &gb_prep;
	sh->cmd_list->Barrier(1, &bg_prep);

	// --- Build Grid ---
	uint32_t total_keys = dx_grid_a_build(sh, state->grid_builder, config, rigid_count, static_count, 
										  state->d_aabb_rigids->GetGPUVirtualAddress(),
										  static_count > 0 ? state->d_aabb_statics->GetGPUVirtualAddress() : 0);

	D3D12_GLOBAL_BARRIER gb_build = {};
	gb_build.SyncBefore = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb_build.SyncAfter = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb_build.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	gb_build.AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;

	D3D12_BARRIER_GROUP bg_build = {};
	bg_build.Type = D3D12_BARRIER_TYPE_GLOBAL;
	bg_build.NumBarriers = 1;
	bg_build.pGlobalBarriers = &gb_build;
	sh->cmd_list->Barrier(1, &bg_build);

	dx_profile_step(&prof, sh, "build");

	if (total_keys > 0) {
		sh->cmd_list->SetComputeRootSignature(state->root_sig);
		
		// The command list was reset during dx_grid_a_build, rebind everything
		bind_universal_context(sh->cmd_list, sh, state, static_count);

		uint32_t constants_bp[5] = { rigid_count, rigid_count, static_count, kernel_max, 0 };
		sh->cmd_list->SetComputeRoot32BitConstants(0, 5, constants_bp, 0);

		uint32_t dispatch_stride = 2048 * block_size;
		uint32_t grid_constants[11] = {
			(uint32_t)config->res_x, (uint32_t)config->res_y, (uint32_t)config->res_z,
			*(uint32_t*)&config->origin_x, *(uint32_t*)&config->origin_y, *(uint32_t*)&config->origin_z,
			*(uint32_t*)&config->cell_size, rigid_count, static_count, total_keys, dispatch_stride
		};
		sh->cmd_list->SetComputeRoot32BitConstants(1, 11, grid_constants, 0);
		
		// Set AABB prep UAV to null safely
		sh->cmd_list->SetComputeRootUnorderedAccessView(12, 0); // u0

		// --- Broad Phase ---
		PIXBeginEvent(sh->cmd_list, PIX_COLOR(255, 128, 0), "Phase: Broad Phase");
		sh->cmd_list->SetPipelineState(state->pso_broad_phase);
		sh->cmd_list->Dispatch(2048, 1, 1);
		PIXEndEvent(sh->cmd_list);
		dx_profile_step(&prof, sh, "query");

		// --- Init Graph (Clamp count) ---
		sh->cmd_list->SetPipelineState(state->pso_init_graph);
		sh->cmd_list->Dispatch(1, 1, 1);

		D3D12_GLOBAL_BARRIER gb_init = {};
		gb_init.SyncBefore = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
		gb_init.SyncAfter = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
		gb_init.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
		gb_init.AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE; // Required for DispatchGraph

		D3D12_BARRIER_GROUP bg_init = {};
		bg_init.Type = D3D12_BARRIER_TYPE_GLOBAL;
		bg_init.NumBarriers = 1;
		bg_init.pGlobalBarriers = &gb_init;
		sh->cmd_list->Barrier(1, &bg_init);

		// --- Dispatch Work Graph (Narrow Phase) ---
		PIXBeginEvent(sh->cmd_list, PIX_COLOR(0, 255, 0), "Phase: Work Graph");

		ID3D12GraphicsCommandList10* cmd_list10 = nullptr;
		sh->cmd_list->QueryInterface(IID_PPV_ARGS(&cmd_list10));

		D3D12_SET_PROGRAM_DESC set_prog = {};
		set_prog.Type = D3D12_PROGRAM_TYPE_WORK_GRAPH;
		set_prog.WorkGraph.ProgramIdentifier = state->wg_program_id;
		set_prog.WorkGraph.Flags = init_backing_mem ? D3D12_SET_WORK_GRAPH_FLAG_INITIALIZE 
													: D3D12_SET_WORK_GRAPH_FLAG_NONE;
		set_prog.WorkGraph.BackingMemory.StartAddress = mem_req.MaxSizeInBytes > 0 
			? state->d_backing_mem->GetGPUVirtualAddress() : 0;
		set_prog.WorkGraph.BackingMemory.SizeInBytes = mem_req.MaxSizeInBytes;
		
		cmd_list10->SetProgram(&set_prog);

		D3D12_DISPATCH_GRAPH_DESC dispatch_desc = {};
		dispatch_desc.Mode = D3D12_DISPATCH_MODE_NODE_GPU_INPUT;
		dispatch_desc.NodeGPUInput = state->d_gpu_input->GetGPUVirtualAddress();

		cmd_list10->DispatchGraph(&dispatch_desc);
		cmd_list10->Release();

		PIXEndEvent(sh->cmd_list);
		dx_profile_step(&prof, sh, "work_graph");
	}

	// --- Readback ---
	D3D12_GLOBAL_BARRIER gb_count = {};
	gb_count.SyncBefore = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb_count.SyncAfter = D3D12_BARRIER_SYNC_COPY;
	gb_count.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	gb_count.AccessAfter = D3D12_BARRIER_ACCESS_COPY_SOURCE;

	D3D12_BARRIER_GROUP bg_count = {};
	bg_count.Type = D3D12_BARRIER_TYPE_GLOBAL;
	bg_count.NumBarriers = 1;
	bg_count.pGlobalBarriers = &gb_count;
	sh->cmd_list->Barrier(1, &bg_count);

	sh->cmd_list->CopyBufferRegion(sh->rb_col_count, 0, sh->d_col_count, 0, sizeof(uint32_t));

	execute_and_wait(sh);

	count = shared_read_count(sh->rb_col_count);
	dx_collision_compact* h_cols = nullptr;

	if (count > 0) {
		dx_profile_step(&prof, sh, "gap_readback");
		PIXBeginEvent(sh->cmd_list, PIX_COLOR(0, 0, 255), "Phase: Readback");

		sh->cmd_list->CopyBufferRegion(sh->rb_collisions, 0, sh->d_collisions, 0,
									   count * sizeof(dx_collision_compact));

		PIXEndEvent(sh->cmd_list);
		dx_profile_step(&prof, sh, "readback");

		execute_and_wait(sh);

		h_cols = shared_read_collisions(sh->rb_collisions, count, &state->h_cols,
										&state->h_cols_capacity);
		*out_count = count;
	}

	dx_profile_end(&prof, sh);

	static dx_profile_acc prof_acc;
	static bool prof_init = false;
	if (!prof_init) {
		dx_profile_acc_init(&prof_acc);
		prof_init = true;
	}
	dx_profile_log(&prof, &prof_acc, "work_graphs", 10);

	return h_cols;
}
