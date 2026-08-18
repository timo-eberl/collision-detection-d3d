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
	uint total_keys; uint dispatch_stride;
};

StructuredBuffer<dx_entity> entities_srv : register(t0);
StructuredBuffer<dx_entity> statics_srv : register(t1);
StructuredBuffer<dx_shape> shapes_srv : register(t2);
StructuredBuffer<packed_aabb> aabb_rigids_srv : register(t3);
StructuredBuffer<packed_aabb> aabb_statics_srv : register(t4);

StructuredBuffer<uint> sorted_keys_srv : register(t5);
StructuredBuffer<packed_aabb> sorted_aabbs_srv : register(t6);
StructuredBuffer<uint> sorted_indices_srv : register(t7);
StructuredBuffer<uint> sorted_vals_srv : register(t8);
StructuredBuffer<uint> cell_ends_srv : register(t9);

RWStructuredBuffer<packed_aabb> aabb_uav : register(u0);
RWStructuredBuffer<dx_potential_pair> potential_pairs_uav : register(u1);

// D3D12_NODE_GPU_INPUT structure
// Byte 0: EntrypointIndex
// Byte 4: NumRecords (Atomic Counter used by Broad Phase)
// Byte 8: RecordsAddress
// Byte 16: RecordsStride
RWByteAddressBuffer gpu_input_buf : register(u2);

RWStructuredBuffer<dx_collision_compact> collisions_uav : register(u3);
RWStructuredBuffer<uint> col_count_uav : register(u4);

[Shader("compute")]
[numthreads(256, 1, 1)]
void cs_aabb_prep(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= item_count) return;

	dx_entity e = entities_srv[i];
	dx_shape s = shapes_srv[e.shape_index];

	packed_aabb box;
	if (e.shape_type == 0) {
		float radius = s.data.x;
		box.min_x = e.position.x - radius; box.max_x = e.position.x + radius;
		box.min_y = e.position.y - radius; box.max_y = e.position.y + radius;
		box.min_z = e.position.z - radius; box.max_z = e.position.z + radius;
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
		box.min_x = e.position.x - half_size.x; box.max_x = e.position.x + half_size.x;
		box.min_y = e.position.y - half_size.y; box.max_y = e.position.y + half_size.y;
		box.min_z = e.position.z - half_size.z; box.max_z = e.position.z + half_size.z;
	}
	aabb_uav[i] = box;
}

void emit_overlap(uint a_idx, uint b_idx, uint b_type) {
	uint idx;
	// Atomically increment the D3D12_NODE_GPU_INPUT.NumRecords directly
	gpu_input_buf.InterlockedAdd(4, 1, idx);
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

// Ensure the Node NumRecords doesn't exceed our physically allocated pair buffer
[Shader("compute")]
[numthreads(1, 1, 1)]
void cs_init_graph(uint3 DTid : SV_DispatchThreadID) {
	uint count = gpu_input_buf.Load(4);
	if (count > max_collisions) {
		gpu_input_buf.Store(4, max_collisions);
	}
}

[Shader("node")]
[NodeLaunch("thread")]
[NodeIsProgramEntry]
void RoutePairs(
	ThreadNodeInputRecord<dx_potential_pair> input,
	[NodeID("Narrow_Sph_Sph")] [MaxRecords(1)] NodeOutput<dx_potential_pair> out_sph_sph,
	[NodeID("Narrow_Sph_Cap")] [MaxRecordsSharedWith(out_sph_sph)] NodeOutput<dx_potential_pair> out_sph_cap,
	[NodeID("Narrow_Sph_Box")] [MaxRecordsSharedWith(out_sph_sph)] NodeOutput<dx_potential_pair> out_sph_box,
	[NodeID("Narrow_Cap_Cap")] [MaxRecordsSharedWith(out_sph_sph)] NodeOutput<dx_potential_pair> out_cap_cap,
	[NodeID("Narrow_Cap_Box")] [MaxRecordsSharedWith(out_sph_sph)] NodeOutput<dx_potential_pair> out_cap_box,
	[NodeID("Narrow_Box_Box")] [MaxRecordsSharedWith(out_sph_sph)] NodeOutput<dx_potential_pair> out_box_box
) {
	dx_potential_pair p = input.Get();
	uint type_a = entities_srv[p.a_index].shape_type;
	uint type_b = (p.b_type == 1) ? entities_srv[p.b_index].shape_type : statics_srv[p.b_index].shape_type;

	if (type_a > type_b) {
		uint temp = type_a; type_a = type_b; type_b = temp;
	}

	int hit_type = 0;
	if (type_a == 0 && type_b == 0) hit_type = 0;
	else if (type_a == 0 && type_b == 1) hit_type = 1;
	else if (type_a == 0 && type_b == 2) hit_type = 2;
	else if (type_a == 1 && type_b == 1) hit_type = 3;
	else if (type_a == 1 && type_b == 2) hit_type = 4;
	else if (type_a == 2 && type_b == 2) hit_type = 5;

	ThreadNodeOutputRecords<dx_potential_pair> r0 = out_sph_sph.GetThreadNodeOutputRecords(hit_type == 0 ? 1 : 0);
	if (hit_type == 0) r0.Get() = p;
	r0.OutputComplete();

	ThreadNodeOutputRecords<dx_potential_pair> r1 = out_sph_cap.GetThreadNodeOutputRecords(hit_type == 1 ? 1 : 0);
	if (hit_type == 1) r1.Get() = p;
	r1.OutputComplete();

	ThreadNodeOutputRecords<dx_potential_pair> r2 = out_sph_box.GetThreadNodeOutputRecords(hit_type == 2 ? 1 : 0);
	if (hit_type == 2) r2.Get() = p;
	r2.OutputComplete();

	ThreadNodeOutputRecords<dx_potential_pair> r3 = out_cap_cap.GetThreadNodeOutputRecords(hit_type == 3 ? 1 : 0);
	if (hit_type == 3) r3.Get() = p;
	r3.OutputComplete();

	ThreadNodeOutputRecords<dx_potential_pair> r4 = out_cap_box.GetThreadNodeOutputRecords(hit_type == 4 ? 1 : 0);
	if (hit_type == 4) r4.Get() = p;
	r4.OutputComplete();

	ThreadNodeOutputRecords<dx_potential_pair> r5 = out_box_box.GetThreadNodeOutputRecords(hit_type == 5 ? 1 : 0);
	if (hit_type == 5) r5.Get() = p;
	r5.OutputComplete();
}

void write_collision(dx_potential_pair p, dx_collision_full c, bool swapped) {
	if (swapped) {
		c.normal = -c.normal;
		float3 temp = c.point_a; c.point_a = c.point_b; c.point_b = temp;
	}
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

[Shader("node")]
[NodeLaunch("thread")]
void Narrow_Sph_Sph(ThreadNodeInputRecord<dx_potential_pair> input) {
	dx_potential_pair p = input.Get();
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) e_b = entities_srv[p.b_index];
	else e_b = statics_srv[p.b_index];
	
	bool swapped = false;
	if (e_a.shape_type > e_b.shape_type) {
		dx_entity temp_e = e_a; e_a = e_b; e_b = temp_e;
		swapped = true;
	}

	dx_collision_full c;
	if (collision_test_sphere_sphere(e_a, shapes_srv[e_a.shape_index], e_b, shapes_srv[e_b.shape_index], c)) {
		write_collision(p, c, swapped);
	}
}

[Shader("node")]
[NodeLaunch("thread")]
void Narrow_Sph_Cap(ThreadNodeInputRecord<dx_potential_pair> input) {
	dx_potential_pair p = input.Get();
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) e_b = entities_srv[p.b_index];
	else e_b = statics_srv[p.b_index];

	bool swapped = false;
	if (e_a.shape_type > e_b.shape_type) {
		dx_entity temp_e = e_a; e_a = e_b; e_b = temp_e;
		swapped = true;
	}

	dx_collision_full c;
	if (collision_test_sphere_capsule(e_a, shapes_srv[e_a.shape_index], e_b, shapes_srv[e_b.shape_index], c)) {
		write_collision(p, c, swapped);
	}
}

[Shader("node")]
[NodeLaunch("thread")]
void Narrow_Sph_Box(ThreadNodeInputRecord<dx_potential_pair> input) {
	dx_potential_pair p = input.Get();
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) e_b = entities_srv[p.b_index];
	else e_b = statics_srv[p.b_index];

	bool swapped = false;
	if (e_a.shape_type > e_b.shape_type) {
		dx_entity temp_e = e_a; e_a = e_b; e_b = temp_e;
		swapped = true;
	}

	dx_collision_full c;
	if (collision_test_sphere_obb(e_a, shapes_srv[e_a.shape_index], e_b, shapes_srv[e_b.shape_index], c)) {
		write_collision(p, c, swapped);
	}
}

[Shader("node")]
[NodeLaunch("thread")]
void Narrow_Cap_Cap(ThreadNodeInputRecord<dx_potential_pair> input) {
	dx_potential_pair p = input.Get();
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) e_b = entities_srv[p.b_index];
	else e_b = statics_srv[p.b_index];

	bool swapped = false;
	if (e_a.shape_type > e_b.shape_type) {
		dx_entity temp_e = e_a; e_a = e_b; e_b = temp_e;
		swapped = true;
	}

	dx_collision_full c;
	if (collision_test_capsule_capsule(e_a, shapes_srv[e_a.shape_index], e_b, shapes_srv[e_b.shape_index], c)) {
		write_collision(p, c, swapped);
	}
}

[Shader("node")]
[NodeLaunch("thread")]
void Narrow_Cap_Box(ThreadNodeInputRecord<dx_potential_pair> input) {
	dx_potential_pair p = input.Get();
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) e_b = entities_srv[p.b_index];
	else e_b = statics_srv[p.b_index];

	bool swapped = false;
	if (e_a.shape_type > e_b.shape_type) {
		dx_entity temp_e = e_a; e_a = e_b; e_b = temp_e;
		swapped = true;
	}

	dx_collision_full c;
	if (collision_test_capsule_obb(e_a, shapes_srv[e_a.shape_index], e_b, shapes_srv[e_b.shape_index], c)) {
		write_collision(p, c, swapped);
	}
}

[Shader("node")]
[NodeLaunch("thread")]
void Narrow_Box_Box(ThreadNodeInputRecord<dx_potential_pair> input) {
	dx_potential_pair p = input.Get();
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) e_b = entities_srv[p.b_index];
	else e_b = statics_srv[p.b_index];

	bool swapped = false;
	if (e_a.shape_type > e_b.shape_type) {
		dx_entity temp_e = e_a; e_a = e_b; e_b = temp_e;
		swapped = true;
	}

	dx_collision_full c;
	if (collision_test_obb_obb(e_a, shapes_srv[e_a.shape_index], e_b, shapes_srv[e_b.shape_index], c)) {
		write_collision(p, c, swapped);
	}
}
