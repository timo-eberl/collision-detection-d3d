#ifndef PREFIX_SUM_H
#define PREFIX_SUM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <directx/d3d12.h>
// #include <d3d12.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dx_prefix_sum_context dx_prefix_sum_context;

// Compiles and loads the necessary compute pipelines for the prefix sum operations.
// Should be called once during application initialization.
dx_prefix_sum_context* dx_prefix_sum_create(ID3D12Device10* device);

void dx_prefix_sum_destroy(dx_prefix_sum_context* ctx);

// Calculates the exact byte size required for the temporary scratchpad memory.
// The caller must allocate a buffer of at least this size and pass its GPU virtual
// address to the dispatch functions.
size_t dx_prefix_sum_get_temp_storage_size(dx_prefix_sum_context* ctx, uint32_t num_elements);

// Computes an exclusive prefix sum (using the addition operator).
//
// CRITICAL MEMORY ALIGNMENT REQUIREMENT:
// To maximize memory bandwidth, the underlying compute shaders vectorize memory loads and
// stores into 16-byte chunks.
// The buffers bound to data_in and data_out MUST be physically allocated with a capacity padded to
// the next multiple of 16 bytes.
//
// In-place execution is supported (data_in == data_out).
void dx_prefix_sum_exclusive(dx_prefix_sum_context* ctx,
                             ID3D12GraphicsCommandList7* cmd_list,
                             uint32_t num_elements,
                             D3D12_GPU_VIRTUAL_ADDRESS data_in,
                             D3D12_GPU_VIRTUAL_ADDRESS data_out,
                             D3D12_GPU_VIRTUAL_ADDRESS temp_storage_addr);

// Computes an inclusive prefix scan using a maximum operator.
//
// CRITICAL MEMORY ALIGNMENT REQUIREMENT:
// To maximize memory bandwidth, the underlying compute shaders vectorize memory loads and
// stores into 16-byte chunks.
// The buffers bound to data_in and data_out MUST be physically allocated with a capacity padded to
// the next multiple of 16 bytes.
//
// In-place execution is supported (data_in == data_out).
void dx_prefix_sum_inclusive_max(dx_prefix_sum_context* ctx,
                                 ID3D12GraphicsCommandList7* cmd_list,
                                 uint32_t num_elements,
                                 D3D12_GPU_VIRTUAL_ADDRESS data_in,
                                 D3D12_GPU_VIRTUAL_ADDRESS data_out,
                                 D3D12_GPU_VIRTUAL_ADDRESS temp_storage_addr);

#ifdef __cplusplus
}
#endif

#endif
