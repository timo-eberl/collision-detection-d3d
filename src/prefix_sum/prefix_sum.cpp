#include "prefix_sum.h"
#include <stdio.h>
#include <stdlib.h>
#include <dxguids/dxguids.h>

#include "prefix_sum_init.h"
#include "prefix_sum_exc.h"
#include "prefix_sum_inc_max.h"

#define ALIGN_256(size) (((size) + 255) & ~255)

struct dx_prefix_sum_context {
	ID3D12RootSignature* root_sig;
	ID3D12PipelineState* pso_init;
	ID3D12PipelineState* pso_exclusive;
	ID3D12PipelineState* pso_inclusive_max;
};

static ID3D12PipelineState* create_pso(ID3D12Device10* device, ID3D12RootSignature* root_sig,
									   const unsigned char* bytecode, size_t length) {
	D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
	pso_desc.pRootSignature = root_sig;
	pso_desc.CS.pShaderBytecode = bytecode;
	pso_desc.CS.BytecodeLength = length;

	ID3D12PipelineState* pso = NULL;
	HRESULT hr = device->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pso));
	if (FAILED(hr)) {
		fprintf(stderr, "[dx12_prefix_sum] Failed to create compute PSO.\n");
	}
	return pso;
}

extern "C" dx_prefix_sum_context* dx_prefix_sum_create(ID3D12Device10* device) {
	dx_prefix_sum_context* ctx = (dx_prefix_sum_context*)calloc(1, sizeof(dx_prefix_sum_context));

	D3D12_ROOT_PARAMETER rp[5] = {};
	rp[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rp[0].Constants.ShaderRegister = 0;
	rp[0].Constants.RegisterSpace = 0;
	rp[0].Constants.Num32BitValues = 2;

	for (int i = 0; i < 4; ++i) {
		rp[1 + i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		rp[1 + i].Descriptor.ShaderRegister = i;
		rp[1 + i].Descriptor.RegisterSpace = 0;
	}

	D3D12_ROOT_SIGNATURE_DESC rs_desc = { 5, rp, 0, NULL, D3D12_ROOT_SIGNATURE_FLAG_NONE };
	ID3DBlob* rs_blob = NULL;
	ID3DBlob* err_blob = NULL;
	HRESULT hr = D3D12SerializeRootSignature(&rs_desc, D3D_ROOT_SIGNATURE_VERSION_1,
											 &rs_blob, &err_blob);

	if (FAILED(hr)) {
		if (err_blob) {
			fprintf(stderr, "[dx12_prefix_sum] Root Sig Error: %s\n", 
					(char*)err_blob->GetBufferPointer());
			err_blob->Release();
		}
		free(ctx);
		return NULL;
	}

	hr = device->CreateRootSignature(0, rs_blob->GetBufferPointer(), rs_blob->GetBufferSize(),
									 IID_PPV_ARGS(&ctx->root_sig));
	rs_blob->Release();

	ctx->pso_init = create_pso(device, ctx->root_sig, prefix_sum_init, sizeof(prefix_sum_init));
	ctx->pso_exclusive = create_pso(device, ctx->root_sig, prefix_sum_exc, sizeof(prefix_sum_exc));
	ctx->pso_inclusive_max = create_pso(device, ctx->root_sig, prefix_sum_inc_max,
										sizeof(prefix_sum_inc_max));

	return ctx;
}

extern "C" void dx_prefix_sum_destroy(dx_prefix_sum_context* ctx) {
	if (!ctx) return;
	if (ctx->pso_init) ctx->pso_init->Release();
	if (ctx->pso_exclusive) ctx->pso_exclusive->Release();
	if (ctx->pso_inclusive_max) ctx->pso_inclusive_max->Release();
	if (ctx->root_sig) ctx->root_sig->Release();
	free(ctx);
}

extern "C" size_t dx_prefix_sum_get_temp_storage_size(dx_prefix_sum_context* ctx,
													  uint32_t num_elements) {
	if (num_elements == 0) return 0;

	// The HLSL implementation handles blocks of 3072 elements, vectorized into uint4 (16 bytes)
	uint32_t aligned_elements = (num_elements + 3) / 4 * 4;
	uint32_t thread_blocks = (aligned_elements + 3072 - 1) / 3072;

	size_t s_bump = 256;
	size_t s_lookback = ALIGN_256(thread_blocks * sizeof(uint32_t));

	return s_bump + s_lookback;
}

static void dispatch_internal(dx_prefix_sum_context* ctx,
							  ID3D12GraphicsCommandList7* cmd_list,
							  uint32_t num_elements,
							  D3D12_GPU_VIRTUAL_ADDRESS data_in,
							  D3D12_GPU_VIRTUAL_ADDRESS data_out,
							  D3D12_GPU_VIRTUAL_ADDRESS temp_storage,
							  bool is_max) {
	if (num_elements == 0) return;

	uint32_t aligned_elements = (num_elements + 3) / 4 * 4;
	uint32_t vectorized_size = aligned_elements / 4;
	uint32_t thread_blocks = (aligned_elements + 3072 - 1) / 3072;

	D3D12_GPU_VIRTUAL_ADDRESS p_bump = temp_storage;
	D3D12_GPU_VIRTUAL_ADDRESS p_lookback = p_bump + 256;

	cmd_list->SetComputeRootSignature(ctx->root_sig);

	uint32_t constants[2] = { vectorized_size, thread_blocks };
	cmd_list->SetComputeRoot32BitConstants(0, 2, constants, 0);

	cmd_list->SetComputeRootUnorderedAccessView(1, data_in);
	cmd_list->SetComputeRootUnorderedAccessView(2, data_out);
	cmd_list->SetComputeRootUnorderedAccessView(3, p_bump);
	cmd_list->SetComputeRootUnorderedAccessView(4, p_lookback);

	// The init shader maps 1 thread to 1 block state. Launch ceiling(blocks / 256).
	uint32_t init_blocks = (thread_blocks + 255) / 256;
	cmd_list->SetPipelineState(ctx->pso_init);
	cmd_list->Dispatch(init_blocks, 1, 1);

	D3D12_GLOBAL_BARRIER gb = {};
	gb.SyncBefore = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb.SyncAfter = D3D12_BARRIER_SYNC_COMPUTE_SHADING;
	gb.AccessBefore = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
	gb.AccessAfter = D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;

	D3D12_BARRIER_GROUP bg = {};
	bg.Type = D3D12_BARRIER_TYPE_GLOBAL;
	bg.NumBarriers = 1;
	bg.pGlobalBarriers = &gb;
	cmd_list->Barrier(1, &bg);

	// Bypass D3D12 65,535 dispatch limits using 2D grids unpacked inside the shader
	uint32_t max_dim = 65535;
	uint32_t dispatch_x = thread_blocks > max_dim ? max_dim : thread_blocks;
	uint32_t dispatch_y = (thread_blocks + max_dim - 1) / max_dim;

	cmd_list->SetPipelineState(is_max ? ctx->pso_inclusive_max : ctx->pso_exclusive);
	cmd_list->Dispatch(dispatch_x, dispatch_y, 1);
}

extern "C" void dx_prefix_sum_exclusive(dx_prefix_sum_context* ctx,
										ID3D12GraphicsCommandList7* cmd_list,
										uint32_t num_elements,
										D3D12_GPU_VIRTUAL_ADDRESS data_in,
										D3D12_GPU_VIRTUAL_ADDRESS data_out,
										D3D12_GPU_VIRTUAL_ADDRESS temp_storage_addr) {
	dispatch_internal(ctx, cmd_list, num_elements, data_in, data_out, temp_storage_addr, false);
}

extern "C" void dx_prefix_sum_inclusive_max(dx_prefix_sum_context* ctx,
											ID3D12GraphicsCommandList7* cmd_list,
											uint32_t num_elements,
											D3D12_GPU_VIRTUAL_ADDRESS data_in,
											D3D12_GPU_VIRTUAL_ADDRESS data_out,
											D3D12_GPU_VIRTUAL_ADDRESS temp_storage_addr) {
	dispatch_internal(ctx, cmd_list, num_elements, data_in, data_out, temp_storage_addr, true);
}
