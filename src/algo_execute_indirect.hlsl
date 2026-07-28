#include "collision_math.hlsli"

cbuffer Constants : register(b0) {
	uint item_count;
	uint rigid_count;
	uint static_count;
	uint max_collisions;
	uint pair_count;
	uint bin_index;
};

struct dx_indirect_command {
	uint pair_count;
	uint bin_index;
	uint grid_x;
	uint grid_y;
	uint grid_z;
};

StructuredBuffer<dx_entity> entities_srv : register(t0);
StructuredBuffer<dx_entity> statics_srv : register(t1);
StructuredBuffer<dx_shape> shapes_srv : register(t2);
StructuredBuffer<packed_aabb> aabb_rigids_srv : register(t3);
StructuredBuffer<packed_aabb> aabb_statics_srv : register(t4);
StructuredBuffer<dx_potential_pair> potential_pairs_srv : register(t5);

RWStructuredBuffer<packed_aabb> aabb_uav : register(u0);
RWStructuredBuffer<dx_potential_pair> potential_pairs_uav : register(u1);
RWStructuredBuffer<uint> pair_count_uav : register(u2);
RWStructuredBuffer<dx_collision_compact> collisions_uav : register(u3);
RWStructuredBuffer<uint> col_count_uav : register(u4);
RWStructuredBuffer<dx_indirect_command> indirect_args_uav : register(u5);

[numthreads(256, 1, 1)]
void cs_aabb_prep(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= item_count) return;

	dx_entity e = entities_srv[i];
	dx_shape s = shapes_srv[e.shape_index];

	packed_aabb box;
	if (e.shape_type == 0) {
		float radius = s.data.x;
		box.min_x = e.position.x - radius;
		box.max_x = e.position.x + radius;
		box.min_y = e.position.y - radius;
		box.max_y = e.position.y + radius;
		box.min_z = e.position.z - radius;
		box.max_z = e.position.z + radius;
	} else if (e.shape_type == 1) {
		float half_height = s.data.x;
		float radius = s.data.y;
		float3 up = rotate_vector(float3(0.0f, 1.0f, 0.0f), e.rotation);
		float3 p_a = e.position - up * half_height;
		float3 p_b = e.position + up * half_height;
		float3 min_p = min(p_a, p_b) - radius;
		float3 max_p = max(p_a, p_b) + radius;
		box.min_x = min_p.x; box.max_x = max_p.x;
		box.min_y = min_p.y; box.max_y = max_p.y;
		box.min_z = min_p.z; box.max_z = max_p.z;
	} else {
		float3 extents = s.data.xyz;
		float3 a0 = rotate_vector(float3(1.0f, 0.0f, 0.0f), e.rotation);
		float3 a1 = rotate_vector(float3(0.0f, 1.0f, 0.0f), e.rotation);
		float3 a2 = rotate_vector(float3(0.0f, 0.0f, 1.0f), e.rotation);
		float3 half_size = abs(a0) * extents.x + abs(a1) * extents.y + abs(a2) * extents.z;
		box.min_x = e.position.x - half_size.x;
		box.max_x = e.position.x + half_size.x;
		box.min_y = e.position.y - half_size.y;
		box.max_y = e.position.y + half_size.y;
		box.min_z = e.position.z - half_size.z;
		box.max_z = e.position.z + half_size.z;
	}
	aabb_uav[i] = box;
}

[numthreads(256, 1, 1)]
void cs_broad_phase(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= rigid_count) return;

	packed_aabb box_i = aabb_rigids_srv[i];
	uint type_a = entities_srv[i].shape_type;

	for (uint j = i + 1; j < rigid_count; ++j) {
		if (aabb_overlap(box_i, aabb_rigids_srv[j])) {
			uint type_b = entities_srv[j].shape_type;
			uint min_t = min(type_a, type_b);
			uint max_t = max(type_a, type_b);
			// Maps an unordered pair of shape types (min_t, max_t) into a contiguous
			// 1D index [0, 5] using triangular numbers.
			// By binning overlaps immediately, we avoid a separate sorting pass and reduce atomic
			// contention by spreading the writes across 6 distinct counters.
			uint bin_idx = (max_t * (max_t + 1)) / 2 + min_t;

			uint idx;
			InterlockedAdd(pair_count_uav[bin_idx], 1, idx);
			if (idx < max_collisions) {
				dx_potential_pair p;
				p.a_index = i;
				p.b_index = j;
				p.b_type = 1;
				p.pad = 0;
				potential_pairs_uav[bin_idx * max_collisions + idx] = p;
			}
		}
	}

	for (uint k = 0; k < static_count; ++k) {
		if (aabb_overlap(box_i, aabb_statics_srv[k])) {
			uint type_b = statics_srv[k].shape_type;
			uint min_t = min(type_a, type_b);
			uint max_t = max(type_a, type_b);
			uint bin_idx = (max_t * (max_t + 1)) / 2 + min_t;

			uint idx;
			InterlockedAdd(pair_count_uav[bin_idx], 1, idx);
			if (idx < max_collisions) {
				dx_potential_pair p;
				p.a_index = i;
				p.b_index = k;
				p.b_type = 0;
				p.pad = 0;
				potential_pairs_uav[bin_idx * max_collisions + idx] = p;
			}
		}
	}
}

// Converts the 6 atomic counters into 6 Dispatch Arguments matching the Command Signature
[numthreads(6, 1, 1)]
void cs_dispatch_prep(uint3 DTid : SV_DispatchThreadID) {
	uint b = DTid.x;
	if (b >= 6) return;

	uint count = pair_count_uav[b];
	
	// Safety clamping is moved to the GPU so the pipeline never has to stall
	if (count > max_collisions) count = max_collisions;

	dx_indirect_command cmd;
	cmd.pair_count = count;
	cmd.bin_index = b;
	cmd.grid_x = (count + 255) / 256;
	cmd.grid_y = 1;
	cmd.grid_z = 1;

	indirect_args_uav[b] = cmd;
}

[numthreads(256, 1, 1)]
void cs_narrow_phase(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= pair_count) return;

	// The 1D potential pairs buffer is logically partitioned into 6 equal chunks.
	// This static layout avoids the need to compute and store dynamic offsets per frame.
	dx_potential_pair p = potential_pairs_srv[bin_index * max_collisions + i];

	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) {
		e_b = entities_srv[p.b_index];
	} else {
		e_b = statics_srv[p.b_index];
	}

	dx_shape s_a = shapes_srv[e_a.shape_index];
	dx_shape s_b = shapes_srv[e_b.shape_index];

	dx_collision_full c;
	if (evaluate_narrow_phase(e_a, s_a, e_b, s_b, c)) {
		uint idx;
		InterlockedAdd(col_count_uav[0], 1, idx);
		if (idx < max_collisions) {
			dx_collision_compact comp;
			comp.a_index = p.a_index;
			comp.b_index = (p.b_type == 0) ? (p.b_index + rigid_count) : p.b_index;
			comp.depth = c.depth;
			comp.point_a = c.point_a;
			comp.normal = encode_octahedral(c.normal);
			collisions_uav[idx] = comp;
		}
	}
}
