#ifndef RADIX_SORT_H
#define RADIX_SORT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <directx/d3d12.h>
// #include <d3d12.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque context holding the sorting Root Signature and hardware-specific PSOs
typedef struct dx_radix_sort_context dx_radix_sort_context;

// Creates the sorting context. Should be called once during app initialization.
// 'is_amd' determines which pre-compiled hardware profile to load.
dx_radix_sort_context* dx_radix_sort_create(ID3D12Device10* device, bool is_amd);

void dx_radix_sort_destroy(dx_radix_sort_context* ctx);

// CUB-style memory query. Returns the total byte size required for the temp buffer.
// The caller is responsible for allocating a buffer of this size.
size_t dx_radix_sort_get_temp_storage_size(dx_radix_sort_context* ctx, uint32_t num_keys);

// Records the sorting commands into the command list.
// The sort operates in-place on keys_in_out and payloads_in_out.
// temp_storage_addr is the GPU virtual address of the buffer sized by the query function.
void dx_radix_sort_dispatch(dx_radix_sort_context* ctx, 
                            ID3D12GraphicsCommandList7* cmd_list,
                            uint32_t num_keys,
                            D3D12_GPU_VIRTUAL_ADDRESS keys_in_out,
                            D3D12_GPU_VIRTUAL_ADDRESS payloads_in_out,
                            D3D12_GPU_VIRTUAL_ADDRESS temp_storage_addr);

#ifdef __cplusplus
}
#endif

#endif
