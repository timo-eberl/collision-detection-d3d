#include "algo_execute_indirect_aabb_shader.h"
#include "algo_execute_indirect_broad_shader.h"
#include "algo_execute_indirect_narrow_shader.h"
#include "algo_execute_indirect_prep_shader.h"
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

struct dx_indirect_command {
	uint32_t pair_count;
	uint32_t bin_index;
	uint32_t grid_x;
	uint32_t grid_y;
	uint32_t grid_z;
};

typedef struct { float min_x, max_x, min_y, max_y, min_z, max_z; uint32_t shape_type; uint32_t pad; } packed_aabb;

struct dx_state_execute_indirect {
	dx_state_grid_a* grid_builder;

	ID3D12RootSignature* root_sig;
	ID3D12PipelineState* pso_aabb_prep;
	ID3D12PipelineState* pso_broad;
	ID3D12PipelineState* pso_prep;
	ID3D12PipelineState* pso_narrow;
	ID3D12CommandSignature* cmd_signature;

	ID3D12Resource* d_aabb_rigids;
	size_t d_aabb_rigids_size;
	ID3D12Resource* d_aabb_statics;
	size_t d_aabb_statics_size;

	ID3D12Resource* d_potential_pairs;
	size_t d_potential_pairs_size;
	ID3D12Resource* d_pair_count;
	size_t d_pair_count_size;
	ID3D12Resource* rb_pair_count;
	size_t rb_pair_count_size;

	ID3D12Resource* d_indirect_args;
	size_t d_indirect_args_size;

	dx_collision_compact* h_cols;
	size_t h_cols_capacity;
};

static void bind_universal_context(ID3D12GraphicsCommandList7* cmd, dx_shared_state* sh,
								   dx_state_execute_indirect* state, uint32_t static_count) {
	D3D12_GPU_VIRTUAL_ADDRESS rigids = sh->d_rigids->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS statics = static_count > 0 ? sh->d_statics->GetGPUVirtualAddress() : rigids;
	D3D12_GPU_VIRTUAL_ADDRESS aabb_r = state->d_aabb_rigids->GetGPUVirtualAddress();
	D3D12_GPU_VIRTUAL_ADDRESS aabb_s = static_count > 0 ? state->d_aabb_statics->GetGPUVirtualAddress() : aabb_r;

	cmd->SetComputeRootShaderResourceView(2, rigids);
	cmd->SetComputeRootShaderResourceView(3, statics);
	cmd->SetComputeRootShaderResourceView(4, sh->d_shapes->GetGPUVirtualAddress());
	cmd->SetComputeRootShaderResourceView(5, aabb_r);
	cmd->SetComputeRootShaderResourceView(6, aabb_s);
	cmd->SetComputeRootShaderResourceView(7, state->d_potential_pairs->GetGPUVirtualAddress());

	cmd->SetComputeRootShaderResourceView(8, dx_grid_a_get_sorted_keys(state->grid_builder)); 
	cmd->SetComputeRootShaderResourceView(9, dx_grid_a_get_sorted_aabbs(state->grid_builder));
	cmd->SetComputeRootShaderResourceView(10, dx_grid_a_get_sorted_indices(state->grid_builder));
	cmd->SetComputeRootShaderResourceView(11, dx_grid_a_get_sorted_vals(state->grid_builder));
	cmd->SetComputeRootShaderResourceView(12, dx_grid_a_get_cell_ends(state->grid_builder));

	cmd->SetComputeRootUnorderedAccessView(13, 0); // u0 set per-dispatch
	cmd->SetComputeRootUnorderedAccessView(14, state->d_potential_pairs->GetGPUVirtualAddress());
	cmd->SetComputeRootUnorderedAccessView(15, state->d_pair_count->GetGPUVirtualAddress());
	cmd->SetComputeRootUnorderedAccessView(16, sh->d_collisions->GetGPUVirtualAddress());
	cmd->SetComputeRootUnorderedAccessView(17, sh->d_col_count->GetGPUVirtualAddress());
	cmd->SetComputeRootUnorderedAccessView(18, state->d_indirect_args->GetGPUVirtualAddress());
}

extern "C" dx_state_execute_indirect* dx_state_execute_indirect_create(dx_shared_state* sh) {
	dx_state_execute_indirect* s = (dx_state_execute_indirect*)calloc(1, sizeof(dx_state_execute_indirect));

	s->grid_builder = dx_grid_a_create(sh->device, sh->is_amd);

	D3D12_ROOT_PARAMETER root_params[19] = {};

	// b0: Constants
	root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	root_params[0].Constants.ShaderRegister = 0;
	root_params[0].Constants.RegisterSpace = 0;
	root_params[0].Constants.Num32BitValues = 6;
	root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// b1: GridConstants
	root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	root_params[1].Constants.ShaderRegister = 1;
	root_params[1].Constants.RegisterSpace = 0;
	root_params[1].Constants.Num32BitValues = 11;
	root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// SRVs: t0 to t10
	for (int i = 0; i < 11; ++i) {
		root_params[2 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		root_params[2 + i].Descriptor.ShaderRegister = i;
		root_params[2 + i].Descriptor.RegisterSpace = 0;
		root_params[2 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	// UAVs: u0 to u5
	for (int i = 0; i < 6; ++i) {
		root_params[13 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		root_params[13 + i].Descriptor.ShaderRegister = i;
		root_params[13 + i].Descriptor.RegisterSpace = 0;
		root_params[13 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	D3D12_ROOT_SIGNATURE_DESC rs_desc = {};
	rs_desc.NumParameters = 19;
	rs_desc.pParameters = root_params;

	ID3DBlob* signature = nullptr;
	ID3DBlob* error = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(
		&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

	if (FAILED(hr)) {
		if (error) {
			fprintf(stderr, "[dx12] Root Signature Error: %s\n", (char*)error->GetBufferPointer());
			error->Release();
		}
		return s;
	}

	DX_CHECK(sh->device->CreateRootSignature(
		0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&s->root_sig)));
	s->root_sig->SetName(L"ExecuteIndirect_Root_Sig");
	signature->Release();

	D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
	pso_desc.pRootSignature = s->root_sig;

	pso_desc.CS.pShaderBytecode = algo_execute_indirect_aabb_shader;
	pso_desc.CS.BytecodeLength = sizeof(algo_execute_indirect_aabb_shader);
	DX_CHECK(sh->device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&s->pso_aabb_prep)));

	pso_desc.CS.pShaderBytecode = algo_execute_indirect_broad_shader;
	pso_desc.CS.BytecodeLength = sizeof(algo_execute_indirect_broad_shader);
	DX_CHECK(sh->device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&s->pso_broad)));

	pso_desc.CS.pShaderBytecode = algo_execute_indirect_prep_shader;
	pso_desc.CS.BytecodeLength = sizeof(algo_execute_indirect_prep_shader);
	DX_CHECK(sh->device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&s->pso_prep)));

	pso_desc.CS.pShaderBytecode = algo_execute_indirect_narrow_shader;
	pso_desc.CS.BytecodeLength = sizeof(algo_execute_indirect_narrow_shader);
	DX_CHECK(sh->device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&s->pso_narrow)));

	// Setup the exact layout for the indirect command signature
	D3D12_INDIRECT_ARGUMENT_DESC indirect_args[2] = {};
	indirect_args[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
	indirect_args[0].Constant.RootParameterIndex = 0; // Constants reside at root index 0
	// Instruct the GPU to inject the first 2 uints directly into the pair_count and bin_index
	// root constants, avoiding buffer lookups in the narrow phase shader
	indirect_args[0].Constant.DestOffsetIn32BitValues = 4;
	indirect_args[0].Constant.Num32BitValuesToSet = 2;
	indirect_args[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;

	D3D12_COMMAND_SIGNATURE_DESC cmd_desc = {};
	cmd_desc.ByteStride = sizeof(dx_indirect_command); // Must be strictly 20 bytes
	cmd_desc.NumArgumentDescs = 2;
	cmd_desc.pArgumentDescs = indirect_args;
	cmd_desc.NodeMask = 0;

	DX_CHECK(sh->device->CreateCommandSignature(&cmd_desc, s->root_sig, IID_PPV_ARGS(&s->cmd_signature)));

	return s;
}

extern "C" void dx_state_execute_indirect_destroy(dx_state_execute_indirect* s) {
	if (!s) return;
	dx_grid_a_destroy(s->grid_builder);
	if (s->pso_aabb_prep) s->pso_aabb_prep->Release();
	if (s->pso_broad) s->pso_broad->Release();
	if (s->pso_prep) s->pso_prep->Release();
	if (s->pso_narrow) s->pso_narrow->Release();
	if (s->root_sig) s->root_sig->Release();
	if (s->cmd_signature) s->cmd_signature->Release();
	if (s->d_aabb_rigids) s->d_aabb_rigids->Release();
	if (s->d_aabb_statics) s->d_aabb_statics->Release();
	if (s->d_potential_pairs) s->d_potential_pairs->Release();
	if (s->d_pair_count) s->d_pair_count->Release();
	if (s->rb_pair_count) s->rb_pair_count->Release();
	if (s->d_indirect_args) s->d_indirect_args->Release();
	if (s->h_cols) free(s->h_cols);
	free(s);
}

extern "C" dx_collision_compact*
dx_run_execute_indirect(dx_shared_state* sh, dx_state_execute_indirect* state,
						const dx_grid_config* config, const dx_entity* rigids,
					    uint32_t rigid_count, const dx_entity* statics, uint32_t static_count,
					    const dx_shape* shapes, uint32_t shape_count, bool statics_changed,
					    uint32_t* out_count) {
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
	dx_profile_cpu_begin(&prof, sh);

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
					 6 * (size_t)kernel_max, sizeof(dx_potential_pair), D3D12_HEAP_TYPE_DEFAULT,
					 D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	ensure_dx_buffer(sh->device, &state->d_pair_count, &state->d_pair_count_size,
					 6, sizeof(uint32_t), D3D12_HEAP_TYPE_DEFAULT,
					 D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	ensure_dx_buffer(sh->device, &state->rb_pair_count, &state->rb_pair_count_size,
					 6, sizeof(uint32_t), D3D12_HEAP_TYPE_READBACK,
					 D3D12_RESOURCE_FLAG_NONE, 1.0f);
	ensure_dx_buffer(sh->device, &state->d_indirect_args, &state->d_indirect_args_size,
					 6, sizeof(dx_indirect_command), D3D12_HEAP_TYPE_DEFAULT,
					 D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);

	PIXBeginEvent(sh->cmd_list, PIX_COLOR(255, 0, 0), "Phase: Upload Memory");

	sh->cmd_list->CopyBufferRegion(sh->d_rigids, 0, sh->up_rigids, 0,
								   rigid_count * sizeof(dx_entity));
	sh->cmd_list->CopyBufferRegion(sh->d_shapes, 0, sh->up_shapes, 0,
								   shape_count * sizeof(dx_shape));
	if (statics_changed && static_count > 0) {
		sh->cmd_list->CopyBufferRegion(sh->d_statics, 0, sh->up_statics, 0,
									   static_count * sizeof(dx_entity));
	}
	sh->cmd_list->CopyBufferRegion(sh->d_col_count, 0, sh->up_zero, 0, sizeof(uint32_t));
	sh->cmd_list->CopyBufferRegion(state->d_pair_count, 0, sh->up_zero, 0, 6 * sizeof(uint32_t));

	D3D12_GLOBAL_BARRIER gb_upload = {};
	gb_upload.SyncBefore = D3D12_BARRIER_SYNC_COPY;
	gb_upload.SyncAfter = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb_upload.AccessBefore = D3D12_BARRIER_ACCESS_COPY_DEST;
	gb_upload.AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE |
							D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;

	D3D12_BARRIER_GROUP bg_upload = {D3D12_BARRIER_TYPE_GLOBAL, 1, &gb_upload};
	sh->cmd_list->Barrier(1, &bg_upload);

	PIXEndEvent(sh->cmd_list);
	
	execute_and_wait(sh);
	dx_profile_cpu_step(&prof, "upload");
	dx_profile_begin(&prof, sh);

	// --- AABB Prep Phase ---
	sh->cmd_list->SetComputeRootSignature(state->root_sig);
	sh->cmd_list->SetPipelineState(state->pso_aabb_prep);

	bind_universal_context(sh->cmd_list, sh, state, static_count);

	PIXBeginEvent(sh->cmd_list, PIX_COLOR(255, 255, 0), "Phase: AABB Prep");

	uint32_t constants[6] = { rigid_count, rigid_count, static_count, kernel_max, 0, 0 };
	sh->cmd_list->SetComputeRoot32BitConstants(0, 6, constants, 0);

	// Prep Rigids
	sh->cmd_list->SetComputeRootUnorderedAccessView(13, state->d_aabb_rigids->GetGPUVirtualAddress()); // u0
	sh->cmd_list->Dispatch(grid_size, 1, 1);

	// Prep Statics
	if (statics_changed && static_count > 0) {
		constants[0] = static_count;
		sh->cmd_list->SetComputeRoot32BitConstants(0, 6, constants, 0);
		sh->cmd_list->SetComputeRootShaderResourceView(3, sh->d_statics->GetGPUVirtualAddress()); // t1
		sh->cmd_list->SetComputeRootUnorderedAccessView(13, state->d_aabb_statics->GetGPUVirtualAddress()); // u0
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

	D3D12_BARRIER_GROUP bg_prep = {D3D12_BARRIER_TYPE_GLOBAL, 1, &gb_prep};
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
	D3D12_BARRIER_GROUP bg_build = {D3D12_BARRIER_TYPE_GLOBAL, 1, &gb_build};
	sh->cmd_list->Barrier(1, &bg_build);

	dx_profile_step(&prof, sh, "build");

	if (total_keys > 0) {
		// --- Broad Phase (Grid Traversal) ---
		sh->cmd_list->SetComputeRootSignature(state->root_sig);
		sh->cmd_list->SetPipelineState(state->pso_broad);
		
		bind_universal_context(sh->cmd_list, sh, state, static_count);

		uint32_t constants_bp[6] = { rigid_count, rigid_count, static_count, kernel_max, 0, 0 };
		sh->cmd_list->SetComputeRoot32BitConstants(0, 6, constants_bp, 0);

		uint32_t dispatch_stride = 2048 * block_size;
		uint32_t grid_constants[11] = {
			(uint32_t)config->res_x, (uint32_t)config->res_y, (uint32_t)config->res_z,
			*(uint32_t*)&config->origin_x, *(uint32_t*)&config->origin_y, *(uint32_t*)&config->origin_z,
			*(uint32_t*)&config->cell_size, rigid_count, static_count, total_keys, dispatch_stride
		};
		sh->cmd_list->SetComputeRoot32BitConstants(1, 11, grid_constants, 0);

		sh->cmd_list->SetComputeRootUnorderedAccessView(13, 0); // Null u0

		PIXBeginEvent(sh->cmd_list, PIX_COLOR(0, 255, 0), "Phase: Broad Dispatch");
		sh->cmd_list->Dispatch(2048, 1, 1);
		PIXEndEvent(sh->cmd_list);
	}
	
	dx_profile_step(&prof, sh, "query");

	// Barrier to sync InterlockedAdds before reading them in the prep shader, and to transition
	// the potential pairs list to SRV so the narrow phase can consume it
	D3D12_GLOBAL_BARRIER gb_broad = {};
	gb_broad.SyncBefore = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb_broad.SyncAfter = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb_broad.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	gb_broad.AccessAfter = D3D12_BARRIER_ACCESS_SHADER_RESOURCE |
						   D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;

	D3D12_BARRIER_GROUP bg_broad = {D3D12_BARRIER_TYPE_GLOBAL, 1, &gb_broad};
	sh->cmd_list->Barrier(1, &bg_broad);

	// --- Dispatch Prep Phase ---
	sh->cmd_list->SetPipelineState(state->pso_prep);

	PIXBeginEvent(sh->cmd_list, PIX_COLOR(255, 128, 0), "Phase: Dispatch Prep");
	sh->cmd_list->Dispatch(1, 1, 1);
	PIXEndEvent(sh->cmd_list);
	dx_profile_step(&prof, sh, "disp_prep");

	// Transition the indirect arguments buffer to the mandatory format for execution
	D3D12_GLOBAL_BARRIER gb_indirect = {};
	gb_indirect.SyncBefore = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb_indirect.SyncAfter = D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
	gb_indirect.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	gb_indirect.AccessAfter = D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT;

	D3D12_BARRIER_GROUP bg_indirect = {D3D12_BARRIER_TYPE_GLOBAL, 1, &gb_indirect};
	sh->cmd_list->Barrier(1, &bg_indirect);

	// --- Narrow Phase (Execute Indirect) ---
	sh->cmd_list->SetPipelineState(state->pso_narrow);

	// Rebind universe to ensure execution has correct SRVs since they might have decayed
	bind_universal_context(sh->cmd_list, sh, state, static_count);

	PIXBeginEvent(sh->cmd_list, PIX_COLOR(0, 255, 255), "Phase: Execute Indirect Narrow");
	sh->cmd_list->ExecuteIndirect(state->cmd_signature, 6, state->d_indirect_args, 0, nullptr, 0);
	PIXEndEvent(sh->cmd_list);

	dx_profile_step(&prof, sh, "narrow");

	// Synchronize everything that was heavily modified for readback
	D3D12_GLOBAL_BARRIER gb_readback = {};
	gb_readback.SyncBefore = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb_readback.SyncAfter = D3D12_BARRIER_SYNC_COPY;
	gb_readback.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	gb_readback.AccessAfter = D3D12_BARRIER_ACCESS_COPY_SOURCE;

	D3D12_BARRIER_GROUP bg_readback = {D3D12_BARRIER_TYPE_GLOBAL, 1, &gb_readback};
	sh->cmd_list->Barrier(1, &bg_readback);

	// Issue the diagnostic reads simultaneously with the physics reads
	sh->cmd_list->CopyBufferRegion(state->rb_pair_count, 0, state->d_pair_count, 0, 6 * sizeof(uint32_t));
	sh->cmd_list->CopyBufferRegion(sh->rb_col_count, 0, sh->d_col_count, 0, sizeof(uint32_t));

	execute_and_wait(sh);
	dx_profile_cpu_step(&prof, "work");

	dx_profile_end(&prof, sh);

	uint32_t pair_counts[6] = {0};
	void* p_mapped = nullptr;
	// ... (leave interim mapping code as is)
	count = shared_read_count(sh->rb_col_count);

	dx_collision_compact* h_cols = nullptr;
	if (count > 0) {
		PIXBeginEvent(sh->cmd_list, PIX_COLOR(0, 0, 255), "Phase: Readback");
		
		sh->cmd_list->CopyBufferRegion(sh->rb_collisions, 0, sh->d_collisions, 0,
									   count * sizeof(dx_collision_compact));
		
		PIXEndEvent(sh->cmd_list);

		execute_and_wait(sh);
		h_cols = shared_read_collisions(sh->rb_collisions, count, &state->h_cols,
										&state->h_cols_capacity);
		dx_profile_cpu_step(&prof, "download");

		*out_count = count;
	}

	static dx_profile_acc prof_acc;
	static bool prof_init = false;
	if (!prof_init) {
		dx_profile_acc_init(&prof_acc);
		prof_init = true;
	}
	// dx_profile_log_frame(&prof, "execute_indirect");
	dx_profile_log(&prof, &prof_acc, "execute_indirect", 10);

	return h_cols;
}
