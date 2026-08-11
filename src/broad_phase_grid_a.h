#ifndef BROAD_PHASE_GRID_A_H
#define BROAD_PHASE_GRID_A_H

#include "collision_detection_d3d.h"
#include "shared.h"
#include <directx/d3d12.h>

typedef struct dx_state_grid_a dx_state_grid_a;

dx_state_grid_a* dx_grid_a_create(ID3D12Device10* device, bool is_amd);
void dx_grid_a_destroy(dx_state_grid_a* state);

// Builds the multi-cell uniform grid index.
void dx_grid_a_build(dx_shared_state* sh, 
                     dx_state_grid_a* state,
                     const dx_grid_config* config,
                     uint32_t rigid_count, 
                     uint32_t static_count,
                     D3D12_GPU_VIRTUAL_ADDRESS aabb_rigids,
                     D3D12_GPU_VIRTUAL_ADDRESS aabb_statics);

// Accessors for the variant to bind the built grid arrays to its SRVs
D3D12_GPU_VIRTUAL_ADDRESS dx_grid_a_get_sorted_aabbs(dx_state_grid_a* state);
D3D12_GPU_VIRTUAL_ADDRESS dx_grid_a_get_sorted_indices(dx_state_grid_a* state);
D3D12_GPU_VIRTUAL_ADDRESS dx_grid_a_get_sorted_vals(dx_state_grid_a* state);
D3D12_GPU_VIRTUAL_ADDRESS dx_grid_a_get_cell_ends(dx_state_grid_a* state);

#endif
