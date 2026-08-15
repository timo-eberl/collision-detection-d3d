#include "collision_math.hlsli"

cbuffer Constants : register(b0) {
	uint item_count;
	uint rigid_count;
	uint static_count;
	uint max_collisions;
	uint pair_count;
};

cbuffer GridConstants : register(b1) {
	int res_x; int res_y; int res_z;
	float origin_x; float origin_y; float origin_z;
	float cell_size;
	uint grid_rigid_count; uint grid_static_count;
	// the above grid constants during phase 4 need to be the same
	uint total_keys; uint dispatch_stride;
};

StructuredBuffer<dx_entity> entities_srv : register(t0);
StructuredBuffer<dx_entity> statics_srv : register(t1);
StructuredBuffer<dx_shape> shapes_srv : register(t2);
StructuredBuffer<packed_aabb> aabb_rigids_srv : register(t3);
StructuredBuffer<packed_aabb> aabb_statics_srv : register(t4);
StructuredBuffer<dx_potential_pair> potential_pairs_srv : register(t5);

// Expanded SRVs for Flat Traversal
StructuredBuffer<uint> sorted_keys_srv : register(t6);
StructuredBuffer<packed_aabb> sorted_aabbs_srv : register(t7);
StructuredBuffer<uint> sorted_indices_srv : register(t8);
StructuredBuffer<uint> sorted_vals_srv : register(t9);
StructuredBuffer<uint> cell_ends_srv : register(t10);

RWStructuredBuffer<packed_aabb> aabb_uav : register(u0);
RWStructuredBuffer<dx_potential_pair> potential_pairs_uav : register(u1);
RWStructuredBuffer<uint> pair_count_uav : register(u2);
RWStructuredBuffer<dx_collision_compact> collisions_uav : register(u3);
RWStructuredBuffer<uint> col_count_uav : register(u4);

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

// -------------------------------------------------------------
// GRID TRAVERSAL OVERLAP EMITTER
// -------------------------------------------------------------
void emit_overlap(uint a_idx, uint b_idx, uint b_type) {
	uint idx;
	InterlockedAdd(pair_count_uav[0], 1, idx);
	if (idx < max_collisions) {
		dx_potential_pair p;
		p.a_index = a_idx;
		p.b_index = b_idx;
		p.b_type = b_type;
		p.pad = 0;
		potential_pairs_uav[idx] = p;
	}
}
#include "grid_a_traversal.hlsli"
// -------------------------------------------------------------

[numthreads(256, 1, 1)]
void cs_narrow_phase(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= pair_count) return;

	dx_potential_pair p = potential_pairs_srv[i];
	
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
