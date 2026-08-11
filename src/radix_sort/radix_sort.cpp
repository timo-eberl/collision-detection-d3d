#include "radix_sort.h"

#include <stdio.h>
#include <stdlib.h>
#include <dxguids/dxguids.h>

#include "gpusort_nv_init.h"
#include "gpusort_nv_upsweep.h"
#include "gpusort_nv_scan.h"
#include "gpusort_nv_downsweep.h"
#include "gpusort_amd_init.h"
#include "gpusort_amd_upsweep.h"
#include "gpusort_amd_scan.h"
#include "gpusort_amd_downsweep.h"

// Root UAV bindings in D3D12 must be 256-byte aligned
#define ALIGN_256(size) (((size) + 255) & ~255)

struct dx_radix_sort_context {
	ID3D12RootSignature* root_sig;
	ID3D12PipelineState* pso_init;
	ID3D12PipelineState* pso_upsweep;
	ID3D12PipelineState* pso_scan;
	ID3D12PipelineState* pso_downsweep;
	bool is_amd;
	uint32_t part_size;
};

static ID3D12PipelineState* create_pso(ID3D12Device10* device, ID3D12RootSignature* root_sig,
									   const unsigned char* bytecode, size_t length) {
	D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
	pso_desc.pRootSignature = root_sig;
	pso_desc.CS.pShaderBytecode = bytecode;
	pso_desc.CS.BytecodeLength = length;
	
	ID3D12PipelineState* pso = nullptr;
	HRESULT hr = device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso));
	if (FAILED(hr)) {
		fprintf(stderr, "[dx12_sort] Failed to create compute PSO.\n");
	}
	return pso;
}

extern "C" dx_radix_sort_context* dx_radix_sort_create(ID3D12Device10* device, bool is_amd) {
	dx_radix_sort_context* ctx = (dx_radix_sort_context*)calloc(1, sizeof(dx_radix_sort_context));
	ctx->is_amd = is_amd;
	ctx->part_size = is_amd ? 2560 : 7680;

	D3D12_ROOT_PARAMETER rp[8] = {};
	rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rp[0].Constants.ShaderRegister = 0;
	rp[0].Constants.RegisterSpace = 0;
	rp[0].Constants.Num32BitValues = 4;
	
	for (int i = 0; i < 7; ++i) {
		rp[1 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		rp[1 + i].Descriptor.ShaderRegister = i;
		rp[1 + i].Descriptor.RegisterSpace = 0;
	}

	D3D12_ROOT_SIGNATURE_DESC rs_desc = { 8, rp, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE };
	ID3DBlob* rs_blob = nullptr;
	ID3DBlob* err_blob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1,
											 &rs_blob, &err_blob);
	
	if (FAILED(hr)) {
		if (err_blob) {
			fprintf(stderr, "[dx12_sort] Root Sig Error: %s\n", (char*)err_blob->GetBufferPointer());
			err_blob->Release();
		}
		free(ctx);
		return nullptr;
	}

	hr = device->CreateRootSignature(0, rs_blob->GetBufferPointer(), rs_blob->GetBufferSize(),
									 IID_PPV_ARGS(&ctx->root_sig));
	rs_blob->Release();

	if (is_amd) {
		ctx->pso_init = create_pso(device, ctx->root_sig, gpusort_amd_init,
								   sizeof(gpusort_amd_init));
		ctx->pso_upsweep = create_pso(device, ctx->root_sig, gpusort_amd_upsweep,
									  sizeof(gpusort_amd_upsweep));
		ctx->pso_scan = create_pso(device, ctx->root_sig, gpusort_amd_scan,
								   sizeof(gpusort_amd_scan));
		ctx->pso_downsweep = create_pso(device, ctx->root_sig, gpusort_amd_downsweep,
										sizeof(gpusort_amd_downsweep));
	} else {
		ctx->pso_init = create_pso(device, ctx->root_sig, gpusort_nv_init,
								   sizeof(gpusort_nv_init));
		ctx->pso_upsweep = create_pso(device, ctx->root_sig, gpusort_nv_upsweep,
									  sizeof(gpusort_nv_upsweep));
		ctx->pso_scan = create_pso(device, ctx->root_sig, gpusort_nv_scan,
								   sizeof(gpusort_nv_scan));
		ctx->pso_downsweep = create_pso(device, ctx->root_sig, gpusort_nv_downsweep,
										sizeof(gpusort_nv_downsweep));
	}

	return ctx;
}

extern "C" void dx_radix_sort_destroy(dx_radix_sort_context* ctx) {
	if (!ctx) return;
	if (ctx->pso_init) ctx->pso_init->Release();
	if (ctx->pso_upsweep) ctx->pso_upsweep->Release();
	if (ctx->pso_scan) ctx->pso_scan->Release();
	if (ctx->pso_downsweep) ctx->pso_downsweep->Release();
	if (ctx->root_sig) ctx->root_sig->Release();
	free(ctx);
}

extern "C" size_t dx_radix_sort_get_temp_storage_size(dx_radix_sort_context* ctx,
													  uint32_t num_keys) {
	if (num_keys == 0) return 0;

	uint32_t thread_blocks = (num_keys + ctx->part_size - 1) / ctx->part_size;
	
	size_t s_alt_keys    = ALIGN_256(num_keys * sizeof(uint32_t));
	size_t s_alt_vals    = ALIGN_256(num_keys * sizeof(uint32_t));
	size_t s_global_hist = ALIGN_256(256 * 4 * sizeof(uint32_t));
	size_t s_pass_hist   = ALIGN_256(256 * thread_blocks * 4 * sizeof(uint32_t));
	size_t s_index       = ALIGN_256(4 * sizeof(uint32_t));

	return s_alt_keys + s_alt_vals + s_global_hist + s_pass_hist + s_index;
}

extern "C" void dx_radix_sort_dispatch(dx_radix_sort_context* ctx, 
									   ID3D12GraphicsCommandList7* cmd_list,
									   uint32_t num_keys,
									   D3D12_GPU_VIRTUAL_ADDRESS keys_in_out,
									   D3D12_GPU_VIRTUAL_ADDRESS payloads_in_out,
									   D3D12_GPU_VIRTUAL_ADDRESS temp_storage) {
	if (num_keys == 0) return;

	uint32_t thread_blocks = (num_keys + ctx->part_size - 1) / ctx->part_size;

	// Calculate sub-allocation sizes
	size_t s_alt_keys    = ALIGN_256(num_keys * sizeof(uint32_t));
	size_t s_alt_vals    = ALIGN_256(num_keys * sizeof(uint32_t));
	size_t s_global_hist = ALIGN_256(256 * 4 * sizeof(uint32_t));
	size_t s_pass_hist   = ALIGN_256(256 * thread_blocks * 4 * sizeof(uint32_t));

	// Distribute the flat temp_storage buffer across the required arrays
	D3D12_GPU_VIRTUAL_ADDRESS p_alt_keys    = temp_storage;
	D3D12_GPU_VIRTUAL_ADDRESS p_alt_vals    = p_alt_keys + s_alt_keys;
	D3D12_GPU_VIRTUAL_ADDRESS p_global_hist = p_alt_vals + s_alt_vals;
	D3D12_GPU_VIRTUAL_ADDRESS p_pass_hist   = p_global_hist + s_global_hist;
	D3D12_GPU_VIRTUAL_ADDRESS p_index       = p_pass_hist + s_pass_hist;

	D3D12_GPU_VIRTUAL_ADDRESS p_sort = keys_in_out;
	D3D12_GPU_VIRTUAL_ADDRESS p_alt = p_alt_keys;
	D3D12_GPU_VIRTUAL_ADDRESS p_sort_payload = payloads_in_out;
	D3D12_GPU_VIRTUAL_ADDRESS p_alt_payload = p_alt_vals;

	D3D12_GLOBAL_BARRIER gb_uav = {};
	gb_uav.SyncBefore = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb_uav.SyncAfter = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb_uav.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	gb_uav.AccessAfter = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;

	D3D12_BARRIER_GROUP bg_uav = {};
	bg_uav.Type = D3D12_BARRIER_TYPE_GLOBAL;
	bg_uav.NumBarriers = 1;
	bg_uav.pGlobalBarriers = &gb_uav;

	cmd_list->SetComputeRootSignature(ctx->root_sig);

	uint32_t constants[4] = { num_keys, 0, thread_blocks, 0 };
	cmd_list->SetComputeRoot32BitConstants(0, 4, constants, 0);
	cmd_list->SetComputeRootUnorderedAccessView(5, p_global_hist);
	cmd_list->SetComputeRootUnorderedAccessView(6, p_pass_hist);
	cmd_list->SetComputeRootUnorderedAccessView(7, p_index);
	
	cmd_list->SetPipelineState(ctx->pso_init);
	cmd_list->Dispatch(1, 1, 1);
	cmd_list->Barrier(1, &bg_uav);

	// Because radix_passes is exactly 4, the ping-pong swaps guarantee that
	// p_sort and p_sort_payload point back to the original input buffers at the end.
	for (uint32_t pass = 0; pass < 4; ++pass) {
		constants[1] = pass * 8; 
		cmd_list->SetComputeRoot32BitConstants(0, 4, constants, 0);

		cmd_list->SetComputeRootUnorderedAccessView(1, p_sort);
		cmd_list->SetComputeRootUnorderedAccessView(2, p_alt);
		cmd_list->SetComputeRootUnorderedAccessView(3, p_sort_payload);
		cmd_list->SetComputeRootUnorderedAccessView(4, p_alt_payload);

		cmd_list->SetPipelineState(ctx->pso_upsweep);
		cmd_list->Dispatch(thread_blocks, 1, 1);
		cmd_list->Barrier(1, &bg_uav);

		cmd_list->SetPipelineState(ctx->pso_scan);
		cmd_list->Dispatch(256, 1, 1); 
		cmd_list->Barrier(1, &bg_uav);

		cmd_list->SetPipelineState(ctx->pso_downsweep);
		cmd_list->Dispatch(thread_blocks, 1, 1);
		cmd_list->Barrier(1, &bg_uav);

		// Ping-pong buffers
		D3D12_GPU_VIRTUAL_ADDRESS tmp_k = p_sort;
		p_sort = p_alt;
		p_alt = tmp_k;

		D3D12_GPU_VIRTUAL_ADDRESS tmp_p = p_sort_payload;
		p_sort_payload = p_alt_payload;
		p_alt_payload = tmp_p;
	}
}
