#include "collision_math.hlsli"
#include "grid_math.hlsli"

cbuffer GridConstants : register(b0) {
	int res_x; int res_y; int res_z;
	float origin_x; float origin_y; float origin_z;
	float cell_size;
	uint rigid_count; uint static_count;
	uint val_offset;
	uint num_elements; // general limit bounds
};

StructuredBuffer<packed_aabb> aabb_rigids_srv : register(t0);
StructuredBuffer<packed_aabb> aabb_statics_srv : register(t1);

// Flexible SRVs based on phase
StructuredBuffer<uint> keys_in_srv : register(t2);
StructuredBuffer<uint> vals_in_srv : register(t3);

StructuredBuffer<packed_aabb> sorted_aabbs_srv_ro : register(t4);

// Flexible UAVs based on phase
RWStructuredBuffer<uint> keys_out_uav : register(u0);
RWStructuredBuffer<uint> vals_out_uav : register(u1);
RWStructuredBuffer<packed_aabb> sorted_aabbs_uav : register(u2);

[numthreads(256, 1, 1)]
void cs_load_key_value(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= num_elements) return;

	packed_aabb b;
	if (val_offset == 0) {
		b = aabb_rigids_srv[i];
	} else {
		b = aabb_statics_srv[i];
	}

	int cx = cell_coord(b.min_x, origin_x, cell_size, res_x);
	int cy = cell_coord(b.min_y, origin_y, cell_size, res_y);
	int cz = cell_coord(b.min_z, origin_z, cell_size, res_z);

	keys_out_uav[val_offset + i] = cell_index(cx, cy, cz, res_x, res_y);
	vals_out_uav[val_offset + i] = val_offset + i;
}

[numthreads(256, 1, 1)]
void cs_permute_aabbs(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= num_elements) return;

	uint original_idx = vals_in_srv[i];
	packed_aabb b;
	if (original_idx < rigid_count) {
		b = aabb_rigids_srv[original_idx];
	} else {
		b = aabb_statics_srv[original_idx - rigid_count];
	}
	sorted_aabbs_uav[i] = b;
}

[numthreads(256, 1, 1)]
void cs_count_cells(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= num_elements) return;

	packed_aabb b = sorted_aabbs_srv_ro[i];
	int cx0 = cell_coord(b.min_x, origin_x, cell_size, res_x);
	int cy0 = cell_coord(b.min_y, origin_y, cell_size, res_y);
	int cz0 = cell_coord(b.min_z, origin_z, cell_size, res_z);
	int cx1 = cell_coord(b.max_x, origin_x, cell_size, res_x);
	int cy1 = cell_coord(b.max_y, origin_y, cell_size, res_y);
	int cz1 = cell_coord(b.max_z, origin_z, cell_size, res_z);

	keys_out_uav[i] = (cx1 - cx0 + 1) * (cy1 - cy0 + 1) * (cz1 - cz0 + 1);
}

[numthreads(256, 1, 1)]
void cs_assign(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= num_elements) return;

	packed_aabb b = sorted_aabbs_srv_ro[i];
	int cx0 = cell_coord(b.min_x, origin_x, cell_size, res_x);
	int cy0 = cell_coord(b.min_y, origin_y, cell_size, res_y);
	int cz0 = cell_coord(b.min_z, origin_z, cell_size, res_z);
	int cx1 = cell_coord(b.max_x, origin_x, cell_size, res_x);
	int cy1 = cell_coord(b.max_y, origin_y, cell_size, res_y);
	int cz1 = cell_coord(b.max_z, origin_z, cell_size, res_z);

	uint off = keys_in_srv[i];
	for (int cz = cz0; cz <= cz1; ++cz) {
		for (int cy = cy0; cy <= cy1; ++cy) {
			for (int cx = cx0; cx <= cx1; ++cx) {
				keys_out_uav[off] = cell_index(cx, cy, cz, res_x, res_y);
				vals_out_uav[off] = i;
				off++;
			}
		}
	}
}


[numthreads(256, 1, 1)]
void cs_find_boundaries(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= num_elements) return;

	uint key = keys_in_srv[i];
	bool is_last = (i == num_elements - 1) || (keys_in_srv[i + 1] != key);

	if (is_last) {
		keys_out_uav[key] = i + 1;
	}
}

[numthreads(256, 1, 1)]
void cs_clear_cells(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= num_elements) return;
	keys_out_uav[i] = 0;
}
