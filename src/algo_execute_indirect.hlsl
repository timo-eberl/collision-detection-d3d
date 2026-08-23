#include "collision_math.hlsli"

cbuffer Constants : register(b0) {
	uint item_count;
	uint rigid_count;
	uint static_count;
	uint max_collisions;
	uint pair_count;
	uint bin_index;
};

cbuffer GridConstants : register(b1) {
	int res_x; int res_y; int res_z;
	float origin_x; float origin_y; float origin_z;
	float cell_size;
	uint grid_rigid_count; uint grid_static_count;
	uint total_keys; uint dispatch_stride;
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
RWStructuredBuffer<dx_indirect_command> indirect_args_uav : register(u5);

[numthreads(256, 1, 1)]
void cs_aabb_prep(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= item_count) return;

	dx_entity e = entities_srv[i];
	dx_shape s = shapes_srv[e.shape_index];

	packed_aabb box = (packed_aabb)0;
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
	box.shape_type = e.shape_type;
	box.pad = 0;
	aabb_uav[i] = box;
}

void emit_overlap(uint a_idx, uint b_idx, uint b_type, uint type_a, uint type_b) {
	uint min_t = min(type_a, type_b);
	uint max_t = max(type_a, type_b);
	uint bin_idx = (max_t * (max_t + 1)) / 2 + min_t;

	uint idx;
	InterlockedAdd(pair_count_uav[bin_idx], 1, idx);
	if (idx < max_collisions) {
		dx_potential_pair p;
		p.a_index = a_idx;
		p.b_index = b_idx;
		p.b_type = b_type;
		p.pad = 0;
		potential_pairs_uav[bin_idx * max_collisions + idx] = p;
	}
}

#include "grid_a_traversal.hlsli"

[numthreads(6, 1, 1)]
void cs_dispatch_prep(uint3 DTid : SV_DispatchThreadID) {
	uint b = DTid.x;
	if (b >= 6) return;

	uint count = pair_count_uav[b];

	if (count > max_collisions) count = max_collisions;

	dx_indirect_command cmd;
	cmd.pair_count = count;
	cmd.bin_index = b;
	cmd.grid_x = (count + 255) / 256;
	cmd.grid_y = 1;
	cmd.grid_z = 1;

	indirect_args_uav[b] = cmd;
}

void write_collision(dx_potential_pair p, float depth, float3 normal, float3 point_a,
					 float3 point_b, bool swapped) {
	if (swapped) {
		normal = -normal;
		float3 temp = point_a; point_a = point_b; point_b = temp;
	}
	uint idx;
	InterlockedAdd(col_count_uav[0], 1, idx);
	if (idx < max_collisions) {
		dx_collision_compact comp;
		comp.a_index = p.a_index;
		comp.b_index = (p.b_type == 0) ? (p.b_index + rigid_count) : p.b_index;
		comp.depth = depth;
		comp.point_a = point_a;
		comp.normal = encode_octahedral(normal);
		collisions_uav[idx] = comp;
	}
}

[numthreads(256, 1, 1)]
void cs_narrow_sph_sph(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= pair_count) return;

	dx_potential_pair p = potential_pairs_srv[bin_index * max_collisions + i];
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) e_b = entities_srv[p.b_index];
	else e_b = statics_srv[p.b_index];
	dx_shape s_a = shapes_srv[e_a.shape_index];
	dx_shape s_b = shapes_srv[e_b.shape_index];

	bool swapped = e_a.shape_type > e_b.shape_type;
	float3 p_a = swapped ? e_b.position : e_a.position;
	float3 p_b = swapped ? e_a.position : e_b.position;
	float r_a = swapped ? s_b.data.x : s_a.data.x;
	float r_b = swapped ? s_a.data.x : s_b.data.x;

	float depth; float3 normal; float3 pt_a; float3 pt_b;
	if (collision_test_sphere_sphere(p_a, r_a, p_b, r_b, depth, normal, pt_a, pt_b)) {
		write_collision(p, depth, normal, pt_a, pt_b, swapped);
	}
}

[numthreads(256, 1, 1)]
void cs_narrow_sph_cap(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= pair_count) return;

	dx_potential_pair p = potential_pairs_srv[bin_index * max_collisions + i];
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) e_b = entities_srv[p.b_index];
	else e_b = statics_srv[p.b_index];
	dx_shape s_a = shapes_srv[e_a.shape_index];
	dx_shape s_b = shapes_srv[e_b.shape_index];

	bool swapped = e_a.shape_type > e_b.shape_type;
	float3 p_sph = swapped ? e_b.position : e_a.position;
	float r_sph = swapped ? s_b.data.x : s_a.data.x;
	float3 p_cap = swapped ? e_a.position : e_b.position;
	float4 rot_cap = swapped ? e_a.rotation : e_b.rotation;
	float hh_cap = swapped ? s_a.data.x : s_b.data.x;
	float r_cap = swapped ? s_a.data.y : s_b.data.y;

	float depth; float3 normal; float3 pt_a; float3 pt_b;
	if (collision_test_sphere_capsule(p_sph, r_sph, p_cap, rot_cap, hh_cap, r_cap,
									  depth, normal, pt_a, pt_b)) {
		write_collision(p, depth, normal, pt_a, pt_b, swapped);
	}
}

[numthreads(256, 1, 1)]
void cs_narrow_cap_cap(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= pair_count) return;

	dx_potential_pair p = potential_pairs_srv[bin_index * max_collisions + i];
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) e_b = entities_srv[p.b_index];
	else e_b = statics_srv[p.b_index];
	dx_shape s_a = shapes_srv[e_a.shape_index];
	dx_shape s_b = shapes_srv[e_b.shape_index];

	bool swapped = e_a.shape_type > e_b.shape_type;
	float3 p_a = swapped ? e_b.position : e_a.position;
	float4 rot_a = swapped ? e_b.rotation : e_a.rotation;
	float hh_a = swapped ? s_b.data.x : s_a.data.x;
	float r_a = swapped ? s_b.data.y : s_a.data.y;
	float3 p_b = swapped ? e_a.position : e_b.position;
	float4 rot_b = swapped ? e_a.rotation : e_b.rotation;
	float hh_b = swapped ? s_a.data.x : s_b.data.x;
	float r_b = swapped ? s_a.data.y : s_b.data.y;

	float depth; float3 normal; float3 pt_a; float3 pt_b;
	if (collision_test_capsule_capsule(p_a, rot_a, hh_a, r_a, p_b, rot_b, hh_b, r_b,
									   depth, normal, pt_a, pt_b)) {
		write_collision(p, depth, normal, pt_a, pt_b, swapped);
	}
}

[numthreads(256, 1, 1)]
void cs_narrow_sph_box(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= pair_count) return;

	dx_potential_pair p = potential_pairs_srv[bin_index * max_collisions + i];
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) e_b = entities_srv[p.b_index];
	else e_b = statics_srv[p.b_index];
	dx_shape s_a = shapes_srv[e_a.shape_index];
	dx_shape s_b = shapes_srv[e_b.shape_index];

	bool swapped = e_a.shape_type > e_b.shape_type;
	float3 p_sph = swapped ? e_b.position : e_a.position;
	float r_sph = swapped ? s_b.data.x : s_a.data.x;
	float3 p_box = swapped ? e_a.position : e_b.position;
	float4 rot_box = swapped ? e_a.rotation : e_b.rotation;
	float3 ext_box = swapped ? s_a.data.xyz : s_b.data.xyz;

	float depth; float3 normal; float3 pt_a; float3 pt_b;
	if (collision_test_sphere_obb(p_sph, r_sph, p_box, rot_box, ext_box,
								  depth, normal, pt_a, pt_b)) {
		write_collision(p, depth, normal, pt_a, pt_b, swapped);
	}
}

[numthreads(256, 1, 1)]
void cs_narrow_cap_box(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= pair_count) return;

	dx_potential_pair p = potential_pairs_srv[bin_index * max_collisions + i];
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) e_b = entities_srv[p.b_index];
	else e_b = statics_srv[p.b_index];
	dx_shape s_a = shapes_srv[e_a.shape_index];
	dx_shape s_b = shapes_srv[e_b.shape_index];

	bool swapped = e_a.shape_type > e_b.shape_type;
	float3 p_cap = swapped ? e_b.position : e_a.position;
	float4 rot_cap = swapped ? e_b.rotation : e_a.rotation;
	float hh_cap = swapped ? s_b.data.x : s_a.data.x;
	float r_cap = swapped ? s_b.data.y : s_a.data.y;
	float3 p_box = swapped ? e_a.position : e_b.position;
	float4 rot_box = swapped ? e_a.rotation : e_b.rotation;
	float3 ext_box = swapped ? s_a.data.xyz : s_b.data.xyz;

	float depth; float3 normal; float3 pt_a; float3 pt_b;
	if (collision_test_capsule_obb(p_cap, rot_cap, hh_cap, r_cap, p_box, rot_box, ext_box,
								   depth, normal, pt_a, pt_b)) {
		write_collision(p, depth, normal, pt_a, pt_b, swapped);
	}
}

[numthreads(256, 1, 1)]
void cs_narrow_box_box(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= pair_count) return;

	dx_potential_pair p = potential_pairs_srv[bin_index * max_collisions + i];
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) e_b = entities_srv[p.b_index];
	else e_b = statics_srv[p.b_index];
	dx_shape s_a = shapes_srv[e_a.shape_index];
	dx_shape s_b = shapes_srv[e_b.shape_index];

	bool swapped = e_a.shape_type > e_b.shape_type;
	float3 p_a = swapped ? e_b.position : e_a.position;
	float4 rot_a = swapped ? e_b.rotation : e_a.rotation;
	float3 ext_a = swapped ? s_b.data.xyz : s_a.data.xyz;
	float3 p_b = swapped ? e_a.position : e_b.position;
	float4 rot_b = swapped ? e_a.rotation : e_b.rotation;
	float3 ext_b = swapped ? s_a.data.xyz : s_b.data.xyz;

	float depth; float3 normal; float3 pt_a; float3 pt_b;
	if (collision_test_obb_obb(p_a, rot_a, ext_a, p_b, rot_b, ext_b,
							   depth, normal, pt_a, pt_b)) {
		write_collision(p, depth, normal, pt_a, pt_b, swapped);
	}
}
