#include "broad_phase_grid_a.h"
#include "radix_sort.h"
#include "prefix_sum.h"
#include "dx_profile.h"

#include "grid_a_load_shader.h"
#include "grid_a_permute_shader.h"
#include "grid_a_count_shader.h"
#include "grid_a_assign_shader.h"
#include "grid_a_bounds_shader.h"
#include "grid_a_clear_shader.h" // Includes the new clear shader

#include <string.h>

struct dx_state_grid_a {
	dx_radix_sort_context* sort_ctx;
	dx_prefix_sum_context* scan_ctx;

	ID3D12RootSignature* root_sig;
	ID3D12PipelineState* pso_load;
	ID3D12PipelineState* pso_permute;
	ID3D12PipelineState* pso_count;
	ID3D12PipelineState* pso_assign;
	ID3D12PipelineState* pso_bounds;
	ID3D12PipelineState* pso_clear;

	ID3D12Resource* d_pre_keys_in; size_t d_pre_keys_in_size;
	ID3D12Resource* d_pre_keys_out; size_t d_pre_keys_out_size;
	ID3D12Resource* d_pre_vals_in; size_t d_pre_vals_in_size;
	ID3D12Resource* d_pre_vals_out; size_t d_pre_vals_out_size;
	
	ID3D12Resource* d_sorted_aabbs; size_t d_sorted_aabbs_size;
	
	ID3D12Resource* d_counts; size_t d_counts_size;
	ID3D12Resource* d_offsets; size_t d_offsets_size;
	
	ID3D12Resource* d_keys_in; size_t d_keys_in_size;
	ID3D12Resource* d_keys_out; size_t d_keys_out_size;
	ID3D12Resource* d_vals_in; size_t d_vals_in_size;
	ID3D12Resource* d_vals_out; size_t d_vals_out_size;

	ID3D12Resource* d_cell_ends; size_t d_cell_ends_size;

	ID3D12Resource* d_temp; size_t d_temp_size;
	ID3D12Resource* rb_scan_ends; size_t rb_scan_ends_size;
};

// Internal barrier helper
static void issue_barrier(ID3D12GraphicsCommandList7* cmd, D3D12_BARRIER_SYNC sync_before,
						  D3D12_BARRIER_SYNC sync_after, D3D12_BARRIER_ACCESS acc_before,
						  D3D12_BARRIER_ACCESS acc_after) {
	D3D12_GLOBAL_BARRIER gb = {};
	gb.SyncBefore = sync_before;
	gb.SyncAfter = sync_after;
	gb.AccessBefore = acc_before;
	gb.AccessAfter = acc_after;
	D3D12_BARRIER_GROUP bg = {D3D12_BARRIER_TYPE_GLOBAL, 1, &gb};
	cmd->Barrier(1, &bg);
}

dx_state_grid_a* dx_grid_a_create(ID3D12Device10* device, bool is_amd) {
	dx_state_grid_a* s = (dx_state_grid_a*)calloc(1, sizeof(dx_state_grid_a));

	s->sort_ctx = dx_radix_sort_create(device, is_amd);
	s->scan_ctx = dx_prefix_sum_create(device);

	D3D12_ROOT_PARAMETER root_params[8] = {};
	
	// b0 (idx 0)
	root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	root_params[0].Constants.ShaderRegister = 0;
	root_params[0].Constants.RegisterSpace = 0;
	root_params[0].Constants.Num32BitValues = 11;
	root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	// SRVs: t0 to t3 (idx 1 to 4)
	for (int i = 0; i < 4; ++i) {
		root_params[1 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		root_params[1 + i].Descriptor.ShaderRegister = i;
		root_params[1 + i].Descriptor.RegisterSpace = 0;
		root_params[1 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	// UAVs: u0 to u2 (idx 5 to 7)
	for (int i = 0; i < 3; ++i) {
		root_params[5 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		root_params[5 + i].Descriptor.ShaderRegister = i;
		root_params[5 + i].Descriptor.RegisterSpace = 0;
		root_params[5 + i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	}

	D3D12_ROOT_SIGNATURE_DESC rs_desc = {};
	rs_desc.NumParameters = 8;
	rs_desc.pParameters = root_params;

	ID3DBlob* sig = nullptr;
	ID3DBlob* error = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1, &sig, &error);

	if (FAILED(hr)) {
		if (error) {
			fprintf(stderr, "[dx12] Root Signature Error: %s\n", (char*)error->GetBufferPointer());
			error->Release();
		}
		return s;
	}

	DX_CHECK(device->CreateRootSignature(0, sig->GetBufferPointer(), sig->GetBufferSize(), IID_PPV_ARGS(&s->root_sig)));
	sig->Release();

	D3D12_COMPUTE_PIPELINE_STATE_DESC pd = {};
	pd.pRootSignature = s->root_sig;

	// Use DX_CHECK to explicitly capture any silent layout incompatibilities in the future
	pd.CS = {grid_a_load_shader, sizeof(grid_a_load_shader)};
	DX_CHECK(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&s->pso_load)));

	pd.CS = {grid_a_permute_shader, sizeof(grid_a_permute_shader)};
	DX_CHECK(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&s->pso_permute)));

	pd.CS = {grid_a_count_shader, sizeof(grid_a_count_shader)};
	DX_CHECK(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&s->pso_count)));

	pd.CS = {grid_a_assign_shader, sizeof(grid_a_assign_shader)};
	DX_CHECK(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&s->pso_assign)));

	pd.CS = {grid_a_bounds_shader, sizeof(grid_a_bounds_shader)};
	DX_CHECK(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&s->pso_bounds)));

	pd.CS = {grid_a_clear_shader, sizeof(grid_a_clear_shader)};
	DX_CHECK(device->CreateComputePipelineState(&pd, IID_PPV_ARGS(&s->pso_clear)));

	return s;
}

void dx_grid_a_destroy(dx_state_grid_a* s) {
	if (!s) return;
	dx_radix_sort_destroy(s->sort_ctx);
	dx_prefix_sum_destroy(s->scan_ctx);

	if (s->root_sig) s->root_sig->Release();
	if (s->pso_load) s->pso_load->Release();
	if (s->pso_permute) s->pso_permute->Release();
	if (s->pso_count) s->pso_count->Release();
	if (s->pso_assign) s->pso_assign->Release();
	if (s->pso_bounds) s->pso_bounds->Release();
	if (s->pso_clear) s->pso_clear->Release();

	if (s->d_pre_keys_in) s->d_pre_keys_in->Release();
	if (s->d_pre_keys_out) s->d_pre_keys_out->Release();
	if (s->d_pre_vals_in) s->d_pre_vals_in->Release();
	if (s->d_pre_vals_out) s->d_pre_vals_out->Release();
	if (s->d_sorted_aabbs) s->d_sorted_aabbs->Release();
	if (s->d_counts) s->d_counts->Release();
	if (s->d_offsets) s->d_offsets->Release();
	if (s->d_keys_in) s->d_keys_in->Release();
	if (s->d_keys_out) s->d_keys_out->Release();
	if (s->d_vals_in) s->d_vals_in->Release();
	if (s->d_vals_out) s->d_vals_out->Release();
	if (s->d_cell_ends) s->d_cell_ends->Release();
	if (s->d_temp) s->d_temp->Release();
	if (s->rb_scan_ends) s->rb_scan_ends->Release();
	
	free(s);
}

void dx_grid_a_build(dx_shared_state* sh, dx_state_grid_a* s,
                     const dx_grid_config* config, uint32_t rigid_count,
                     uint32_t static_count, D3D12_GPU_VIRTUAL_ADDRESS aabb_rigids,
                     D3D12_GPU_VIRTUAL_ADDRESS aabb_statics) {
	uint32_t total_bodies = rigid_count + static_count;
	uint32_t total_padded = (total_bodies + 3) & ~3; 

	uint32_t constants[11] = {
		(uint32_t)config->res_x, (uint32_t)config->res_y, (uint32_t)config->res_z,
		*(uint32_t*)&config->origin_x, *(uint32_t*)&config->origin_y, *(uint32_t*)&config->origin_z,
		*(uint32_t*)&config->cell_size, rigid_count, static_count, 0, total_bodies
	};

	ensure_dx_buffer(sh->device, &s->d_pre_keys_in, &s->d_pre_keys_in_size, total_padded, sizeof(uint32_t), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	ensure_dx_buffer(sh->device, &s->d_pre_keys_out, &s->d_pre_keys_out_size, total_padded, sizeof(uint32_t), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	ensure_dx_buffer(sh->device, &s->d_pre_vals_in, &s->d_pre_vals_in_size, total_padded, sizeof(uint32_t), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	ensure_dx_buffer(sh->device, &s->d_pre_vals_out, &s->d_pre_vals_out_size, total_padded, sizeof(uint32_t), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);

	sh->cmd_list->SetComputeRootSignature(s->root_sig);
	
	// Phase 0a: Load
	sh->cmd_list->SetPipelineState(s->pso_load);
	constants[9] = 0; constants[10] = rigid_count;
	sh->cmd_list->SetComputeRoot32BitConstants(0, 11, constants, 0);
	sh->cmd_list->SetComputeRootShaderResourceView(1, aabb_rigids); // t0
	sh->cmd_list->SetComputeRootUnorderedAccessView(5, s->d_pre_keys_in->GetGPUVirtualAddress()); // u0
	sh->cmd_list->SetComputeRootUnorderedAccessView(6, s->d_pre_vals_in->GetGPUVirtualAddress()); // u1
	sh->cmd_list->Dispatch((rigid_count + 255) / 256, 1, 1);

	if (static_count > 0) {
		constants[9] = rigid_count; constants[10] = static_count;
		sh->cmd_list->SetComputeRoot32BitConstants(0, 11, constants, 0);
		sh->cmd_list->SetComputeRootShaderResourceView(2, aabb_statics); // t1
		sh->cmd_list->Dispatch((static_count + 255) / 256, 1, 1);
	}

	issue_barrier(sh->cmd_list, D3D12_BARRIER_SYNC_COMPUTE_SHADING, D3D12_BARRIER_SYNC_COMPUTE_SHADING,
				  D3D12_BARRIER_ACCESS_UNORDERED_ACCESS, D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_UNORDERED_ACCESS);

	// Phase 0b: Sort
	size_t t_sort = dx_radix_sort_get_temp_storage_size(s->sort_ctx, total_bodies);
	ensure_dx_buffer(sh->device, &s->d_temp, &s->d_temp_size, t_sort, 1, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.5f);
	dx_radix_sort_dispatch(s->sort_ctx, sh->cmd_list, total_bodies, s->d_pre_keys_in->GetGPUVirtualAddress(),
						   s->d_pre_vals_in->GetGPUVirtualAddress(), s->d_temp->GetGPUVirtualAddress());
	
	issue_barrier(sh->cmd_list, D3D12_BARRIER_SYNC_COMPUTE_SHADING, D3D12_BARRIER_SYNC_COMPUTE_SHADING,
				  D3D12_BARRIER_ACCESS_UNORDERED_ACCESS, D3D12_BARRIER_ACCESS_SHADER_RESOURCE);

	// Phase 0c: Permute
	ensure_dx_buffer(sh->device, &s->d_sorted_aabbs, &s->d_sorted_aabbs_size, total_padded, 24, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	sh->cmd_list->SetComputeRootSignature(s->root_sig);
	sh->cmd_list->SetPipelineState(s->pso_permute);
	constants[10] = total_bodies;
	sh->cmd_list->SetComputeRoot32BitConstants(0, 11, constants, 0);
	sh->cmd_list->SetComputeRootShaderResourceView(1, aabb_rigids); // t0
	if (static_count > 0) sh->cmd_list->SetComputeRootShaderResourceView(2, aabb_statics); // t1
	sh->cmd_list->SetComputeRootShaderResourceView(4, s->d_pre_vals_in->GetGPUVirtualAddress()); // t3
	sh->cmd_list->SetComputeRootUnorderedAccessView(7, s->d_sorted_aabbs->GetGPUVirtualAddress()); // u2
	sh->cmd_list->Dispatch((total_bodies + 255) / 256, 1, 1);

	issue_barrier(sh->cmd_list, D3D12_BARRIER_SYNC_COMPUTE_SHADING, D3D12_BARRIER_SYNC_COMPUTE_SHADING,
				  D3D12_BARRIER_ACCESS_UNORDERED_ACCESS, D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_UNORDERED_ACCESS);

	// Phase 1a: Count
	ensure_dx_buffer(sh->device, &s->d_counts, &s->d_counts_size, total_padded, sizeof(uint32_t), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	sh->cmd_list->SetPipelineState(s->pso_count);
	sh->cmd_list->SetComputeRootUnorderedAccessView(5, s->d_counts->GetGPUVirtualAddress()); // u0 (keys_out_uav)
	sh->cmd_list->SetComputeRootUnorderedAccessView(7, s->d_sorted_aabbs->GetGPUVirtualAddress()); // u2
	sh->cmd_list->Dispatch((total_bodies + 255) / 256, 1, 1);

	issue_barrier(sh->cmd_list, D3D12_BARRIER_SYNC_COMPUTE_SHADING, D3D12_BARRIER_SYNC_COMPUTE_SHADING,
				  D3D12_BARRIER_ACCESS_UNORDERED_ACCESS, D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_UNORDERED_ACCESS);

	// Phase 1b: Scan
	ensure_dx_buffer(sh->device, &s->d_offsets, &s->d_offsets_size, total_padded, sizeof(uint32_t), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	size_t t_scan = dx_prefix_sum_get_temp_storage_size(s->scan_ctx, total_padded);
	ensure_dx_buffer(sh->device, &s->d_temp, &s->d_temp_size, t_scan, 1, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.5f);
	dx_prefix_sum_exclusive(s->scan_ctx, sh->cmd_list, total_padded, s->d_counts->GetGPUVirtualAddress(),
							s->d_offsets->GetGPUVirtualAddress(), s->d_temp->GetGPUVirtualAddress());

	issue_barrier(sh->cmd_list, D3D12_BARRIER_SYNC_COMPUTE_SHADING, D3D12_BARRIER_SYNC_COPY,
				  D3D12_BARRIER_ACCESS_UNORDERED_ACCESS, D3D12_BARRIER_ACCESS_COPY_SOURCE);

	// CPU Sync logic mimicking CUDA's readback of total sum
	ensure_dx_buffer(sh->device, &s->rb_scan_ends, &s->rb_scan_ends_size, 2, sizeof(uint32_t), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, 1.0f);
	sh->cmd_list->CopyBufferRegion(s->rb_scan_ends, 0, s->d_offsets, (total_bodies - 1) * sizeof(uint32_t), sizeof(uint32_t));
	sh->cmd_list->CopyBufferRegion(s->rb_scan_ends, sizeof(uint32_t), s->d_counts, (total_bodies - 1) * sizeof(uint32_t), sizeof(uint32_t));
	
	execute_and_wait(sh);

	uint32_t ends[2];
	void* mapped = nullptr;
	s->rb_scan_ends->Map(0, nullptr, &mapped);
	memcpy(ends, mapped, 2 * sizeof(uint32_t));
	s->rb_scan_ends->Unmap(0, nullptr);
	
	uint32_t total_keys = ends[0] + ends[1];
	if (total_keys == 0) return; // Prevent dispatching 0 grids
	uint32_t total_keys_padded = (total_keys + 3) & ~3;

	// Back to execution
	sh->cmd_list->SetComputeRootSignature(s->root_sig);
	
	// Phase 1c: Assign
	ensure_dx_buffer(sh->device, &s->d_keys_in, &s->d_keys_in_size, total_keys_padded, sizeof(uint32_t), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	ensure_dx_buffer(sh->device, &s->d_keys_out, &s->d_keys_out_size, total_keys_padded, sizeof(uint32_t), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	ensure_dx_buffer(sh->device, &s->d_vals_in, &s->d_vals_in_size, total_keys_padded, sizeof(uint32_t), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	ensure_dx_buffer(sh->device, &s->d_vals_out, &s->d_vals_out_size, total_keys_padded, sizeof(uint32_t), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);

	sh->cmd_list->SetPipelineState(s->pso_assign);
	sh->cmd_list->SetComputeRoot32BitConstants(0, 11, constants, 0);
	sh->cmd_list->SetComputeRootShaderResourceView(3, s->d_offsets->GetGPUVirtualAddress()); // t2 (keys_in_srv)
	sh->cmd_list->SetComputeRootUnorderedAccessView(5, s->d_keys_in->GetGPUVirtualAddress()); // u0 (keys_out_uav)
	sh->cmd_list->SetComputeRootUnorderedAccessView(6, s->d_vals_in->GetGPUVirtualAddress()); // u1 (vals_out_uav)
	sh->cmd_list->SetComputeRootUnorderedAccessView(7, s->d_sorted_aabbs->GetGPUVirtualAddress()); // u2 (sorted_aabbs_uav)
	sh->cmd_list->Dispatch((total_bodies + 255) / 256, 1, 1);

	issue_barrier(sh->cmd_list, D3D12_BARRIER_SYNC_COMPUTE_SHADING, D3D12_BARRIER_SYNC_COMPUTE_SHADING,
				  D3D12_BARRIER_ACCESS_UNORDERED_ACCESS, D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_UNORDERED_ACCESS);

	// Phase 2: Sort Pairs
	t_sort = dx_radix_sort_get_temp_storage_size(s->sort_ctx, total_keys);
	ensure_dx_buffer(sh->device, &s->d_temp, &s->d_temp_size, t_sort, 1, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.5f);
	dx_radix_sort_dispatch(s->sort_ctx, sh->cmd_list, total_keys, s->d_keys_in->GetGPUVirtualAddress(),
						   s->d_vals_in->GetGPUVirtualAddress(), s->d_temp->GetGPUVirtualAddress());
	
	issue_barrier(sh->cmd_list, D3D12_BARRIER_SYNC_COMPUTE_SHADING, D3D12_BARRIER_SYNC_COMPUTE_SHADING,
				  D3D12_BARRIER_ACCESS_UNORDERED_ACCESS, D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_UNORDERED_ACCESS);

	// Phase 3: Boundaries
	uint32_t num_cells = config->res_x * config->res_y * config->res_z;
	uint32_t cells_padded = (num_cells + 3) & ~3;
	ensure_dx_buffer(sh->device, &s->d_cell_ends, &s->d_cell_ends_size, cells_padded, sizeof(uint32_t), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	
	sh->cmd_list->SetComputeRootSignature(s->root_sig);
	
	// 3a. Clear cell ends 
	sh->cmd_list->SetPipelineState(s->pso_clear);
	constants[10] = cells_padded; // Clear up to padded elements
	sh->cmd_list->SetComputeRoot32BitConstants(0, 11, constants, 0);
	sh->cmd_list->SetComputeRootUnorderedAccessView(5, s->d_cell_ends->GetGPUVirtualAddress()); // u0
	sh->cmd_list->Dispatch((cells_padded + 255) / 256, 1, 1);

	issue_barrier(sh->cmd_list, D3D12_BARRIER_SYNC_COMPUTE_SHADING, D3D12_BARRIER_SYNC_COMPUTE_SHADING,
				  D3D12_BARRIER_ACCESS_UNORDERED_ACCESS, D3D12_BARRIER_ACCESS_UNORDERED_ACCESS);

	// 3b. Boundaries 
	sh->cmd_list->SetPipelineState(s->pso_bounds);
	constants[10] = total_keys; // Revert bound length back to keys
	sh->cmd_list->SetComputeRoot32BitConstants(0, 11, constants, 0);
	sh->cmd_list->SetComputeRootShaderResourceView(3, s->d_keys_in->GetGPUVirtualAddress()); // t2 (keys_in_srv)
	sh->cmd_list->SetComputeRootUnorderedAccessView(5, s->d_cell_ends->GetGPUVirtualAddress()); // u0 (keys_out_uav)
	sh->cmd_list->Dispatch((total_keys + 255) / 256, 1, 1);

	issue_barrier(sh->cmd_list, D3D12_BARRIER_SYNC_COMPUTE_SHADING, D3D12_BARRIER_SYNC_COMPUTE_SHADING,
				  D3D12_BARRIER_ACCESS_UNORDERED_ACCESS, D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_UNORDERED_ACCESS);

	// 3c. Inclusive Scan
	t_scan = dx_prefix_sum_get_temp_storage_size(s->scan_ctx, cells_padded);
	ensure_dx_buffer(sh->device, &s->d_temp, &s->d_temp_size, t_scan, 1, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.5f);
	dx_prefix_sum_inclusive_max(s->scan_ctx, sh->cmd_list, cells_padded, s->d_cell_ends->GetGPUVirtualAddress(),
								s->d_cell_ends->GetGPUVirtualAddress(), s->d_temp->GetGPUVirtualAddress());
}

// Accessors
D3D12_GPU_VIRTUAL_ADDRESS dx_grid_a_get_sorted_aabbs(dx_state_grid_a* s) { return s->d_sorted_aabbs->GetGPUVirtualAddress(); }
D3D12_GPU_VIRTUAL_ADDRESS dx_grid_a_get_sorted_indices(dx_state_grid_a* s) { return s->d_pre_vals_in->GetGPUVirtualAddress(); }
D3D12_GPU_VIRTUAL_ADDRESS dx_grid_a_get_sorted_vals(dx_state_grid_a* s) { return s->d_vals_in->GetGPUVirtualAddress(); }
D3D12_GPU_VIRTUAL_ADDRESS dx_grid_a_get_cell_ends(dx_state_grid_a* s) { return s->d_cell_ends->GetGPUVirtualAddress(); }
