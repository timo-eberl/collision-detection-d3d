#ifndef GRID_A_TRAVERSAL_HLSLI
#define GRID_A_TRAVERSAL_HLSLI

#include "grid_math.hlsli"

// Assumes the variant has provided:
// - void emit_overlap(uint a_idx, uint b_idx, uint b_type)
// - cbuffer GridConstants mapping to grid dimensions and sizes

uint lowest_common_cell(packed_aabb a, packed_aabb b) {
	int ax0 = cell_coord(a.min_x, origin_x, cell_size, res_x);
	int ay0 = cell_coord(a.min_y, origin_y, cell_size, res_y);
	int az0 = cell_coord(a.min_z, origin_z, cell_size, res_z);
	int ax1 = cell_coord(a.max_x, origin_x, cell_size, res_x);
	int ay1 = cell_coord(a.max_y, origin_y, cell_size, res_y);
	int az1 = cell_coord(a.max_z, origin_z, cell_size, res_z);

	int bx0 = cell_coord(b.min_x, origin_x, cell_size, res_x);
	int by0 = cell_coord(b.min_y, origin_y, cell_size, res_y);
	int bz0 = cell_coord(b.min_z, origin_z, cell_size, res_z);
	int bx1 = cell_coord(b.max_x, origin_x, cell_size, res_x);
	int by1 = cell_coord(b.max_y, origin_y, cell_size, res_y);
	int bz1 = cell_coord(b.max_z, origin_z, cell_size, res_z);

	int sx = max(ax0, bx0);
	int sy = max(ay0, by0);
	int sz = max(az0, bz0);

	if (sx > ax1 || sx > bx1) return 0xFFFFFFFFu;
	if (sy > ay1 || sy > by1) return 0xFFFFFFFFu;
	if (sz > az1 || sz > bz1) return 0xFFFFFFFFu;

	return cell_index(sx, sy, sz, res_x, res_y);
}

#ifdef IS_WORK_GRAPHS
[Shader("compute")]
#endif
[numthreads(256, 1, 1)]
void cs_broad_phase(uint3 DTid : SV_DispatchThreadID) {
	uint tid = DTid.x;
	uint stride = dispatch_stride;

	for (uint i = tid; i < total_keys; i += stride) {
		uint ci = sorted_keys_srv[i];
		uint my_sorted_idx = sorted_vals_srv[i];
		uint my_original_idx = sorted_indices_srv[my_sorted_idx];

		if (my_original_idx >= grid_rigid_count) continue;

		packed_aabb ri = sorted_aabbs_srv[my_sorted_idx];

		uint start = (ci == 0) ? 0 : cell_ends_srv[ci - 1];
		uint end = cell_ends_srv[ci];

		for (uint k = start; k < end; ++k) {
			uint neighbor_sorted_idx = sorted_vals_srv[k];
			uint other_idx = sorted_indices_srv[neighbor_sorted_idx];
			
			bool is_rigid = (other_idx < grid_rigid_count);
			if (is_rigid && other_idx <= my_original_idx) continue;

			packed_aabb r_neigh = sorted_aabbs_srv[neighbor_sorted_idx];
			if (!aabb_overlap(ri, r_neigh)) continue;
			if (ci != lowest_common_cell(ri, r_neigh)) continue;

			emit_overlap(my_original_idx, is_rigid ? other_idx : other_idx - grid_rigid_count, 
						 is_rigid ? 1 : 0, ri.shape_type, r_neigh.shape_type);
		}
	}
}
#endif
