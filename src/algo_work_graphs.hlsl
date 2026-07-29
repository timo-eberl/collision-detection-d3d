#include "collision_math.hlsli"

cbuffer Constants : register(b0) {
	uint item_count;
	uint rigid_count;
	uint static_count;
	uint max_collisions;
	uint pair_count;
};

StructuredBuffer<dx_entity> entities_srv : register(t0);
StructuredBuffer<dx_entity> statics_srv : register(t1);
StructuredBuffer<dx_shape> shapes_srv : register(t2);
StructuredBuffer<packed_aabb> aabb_rigids_srv : register(t3);
StructuredBuffer<packed_aabb> aabb_statics_srv : register(t4);

RWStructuredBuffer<packed_aabb> aabb_uav : register(u0);

// u1 acts as the D3D12_NODE_GPU_INPUT backing buffer
// Byte 0: EntrypointIndex
// Byte 4: NumRecords (Atomic Counter)
// Byte 8: RecordsAddress (64-bit)
// Byte 16: RecordsStride (64-bit)
// Byte 32+: BroadPhaseWork Records
RWByteAddressBuffer gpu_input_buf : register(u1);

RWStructuredBuffer<dx_collision_compact> collisions_uav : register(u3);
RWStructuredBuffer<uint> col_count_uav : register(u4);

struct BroadPhaseWork {
	uint a_index;
	uint start_b_index;
};

// --- AABB Prep Phase (Same as naive) ---
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

// --- Graph Init Phase ---
// Calculates chunks and populates D3D12_NODE_GPU_INPUT buffer dynamically
[Shader("compute")]
[numthreads(256, 1, 1)]
void cs_init_graph(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= rigid_count) return;

	uint items_to_check = (rigid_count - 1 - i) + static_count;
	uint chunks = (items_to_check + 63) / 64;

	uint offset;
	gpu_input_buf.InterlockedAdd(4, chunks, offset); // Atomic Add to NumRecords

	for (uint c = 0; c < chunks; ++c) {
		uint record_idx = offset + c;
		uint byte_offset = 32 + record_idx * 8; // Offset past the 32-byte header
		gpu_input_buf.Store2(byte_offset, uint2(i, (i + 1) + c * 64));
	}
}

// --- Work Graph: Broad Phase Router ---
// Evaluates exactly 64 pairs in parallel and routes them to Narrow Phase
[Shader("node")]
[NodeLaunch("broadcasting")]
[NodeIsProgramEntry]
[NumThreads(64, 1, 1)]
[NodeDispatchGrid(1, 1, 1)]
void BroadPhase_Chunk(
	DispatchNodeInputRecord<BroadPhaseWork> input,
	[NodeID("Narrow_Sph_Sph")] [MaxRecords(64)] NodeOutput<dx_potential_pair> out_sph_sph,
	[NodeID("Narrow_Sph_Cap")] [MaxRecordsSharedWith(out_sph_sph)] NodeOutput<dx_potential_pair> out_sph_cap,
	[NodeID("Narrow_Sph_Box")] [MaxRecordsSharedWith(out_sph_sph)] NodeOutput<dx_potential_pair> out_sph_box,
	[NodeID("Narrow_Cap_Cap")] [MaxRecordsSharedWith(out_sph_sph)] NodeOutput<dx_potential_pair> out_cap_cap,
	[NodeID("Narrow_Cap_Box")] [MaxRecordsSharedWith(out_sph_sph)] NodeOutput<dx_potential_pair> out_cap_box,
	[NodeID("Narrow_Box_Box")] [MaxRecordsSharedWith(out_sph_sph)] NodeOutput<dx_potential_pair> out_box_box,
	uint3 DTid : SV_DispatchThreadID
) {
	BroadPhaseWork work = input.Get();
	uint a_idx = work.a_index;
	uint b_iter = work.start_b_index + DTid.x;

	bool valid = false;
	packed_aabb box_b;
	uint b_idx = 0;
	uint b_type_flag = 0;

	if (b_iter < rigid_count) {
		b_idx = b_iter;
		b_type_flag = 1;
		box_b = aabb_rigids_srv[b_idx];
		valid = true;
	} else if (b_iter < rigid_count + static_count) {
		b_idx = b_iter - rigid_count;
		b_type_flag = 0;
		box_b = aabb_statics_srv[b_idx];
		valid = true;
    }

	packed_aabb box_a = aabb_rigids_srv[a_idx];
	int hit_type = -1;

	if (valid && aabb_overlap(box_a, box_b)) {
		uint type_a = entities_srv[a_idx].shape_type;
		uint type_b = (b_type_flag == 1) ? entities_srv[b_idx].shape_type : statics_srv[b_idx].shape_type;

		if (type_a > type_b) {
			uint temp = type_a; type_a = type_b; type_b = temp;
		}

		if (type_a == 0 && type_b == 0) hit_type = 0;
		else if (type_a == 0 && type_b == 1) hit_type = 1;
		else if (type_a == 0 && type_b == 2) hit_type = 2;
		else if (type_a == 1 && type_b == 1) hit_type = 3;
		else if (type_a == 1 && type_b == 2) hit_type = 4;
		else if (type_a == 2 && type_b == 2) hit_type = 5;
	}

	dx_potential_pair p;
	p.a_index = a_idx;
	p.b_index = b_idx;
	p.b_type = b_type_flag;
	p.pad = 0;

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

// --- Work Graph: Narrow Phase Leaves ---
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
	if (p.b_type == 1) {
		e_b = entities_srv[p.b_index];
	} else {
		e_b = statics_srv[p.b_index];
	}

	bool swapped = false;
	if (e_a.shape_type > e_b.shape_type) {
		dx_entity temp_e = e_a;
		e_a = e_b;
		e_b = temp_e;
		swapped = true;
	}

	dx_shape s_a = shapes_srv[e_a.shape_index];
	dx_shape s_b = shapes_srv[e_b.shape_index];
	dx_collision_full c;
	if (collision_test_sphere_sphere(e_a, s_a, e_b, s_b, c)) {
		write_collision(p, c, swapped);
	}
}

[Shader("node")]
[NodeLaunch("thread")]
void Narrow_Sph_Cap(ThreadNodeInputRecord<dx_potential_pair> input) {
	dx_potential_pair p = input.Get();
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) {
		e_b = entities_srv[p.b_index];
	} else {
		e_b = statics_srv[p.b_index];
	}

	bool swapped = false;
	if (e_a.shape_type > e_b.shape_type) {
		dx_entity temp_e = e_a;
		e_a = e_b;
		e_b = temp_e;
		swapped = true;
	}

	dx_shape s_a = shapes_srv[e_a.shape_index];
	dx_shape s_b = shapes_srv[e_b.shape_index];
	dx_collision_full c;
	if (collision_test_sphere_capsule(e_a, s_a, e_b, s_b, c)) {
		write_collision(p, c, swapped);
	}
}

[Shader("node")]
[NodeLaunch("thread")]
void Narrow_Sph_Box(ThreadNodeInputRecord<dx_potential_pair> input) {
	dx_potential_pair p = input.Get();
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) {
		e_b = entities_srv[p.b_index];
	} else {
		e_b = statics_srv[p.b_index];
	}

	bool swapped = false;
	if (e_a.shape_type > e_b.shape_type) {
		dx_entity temp_e = e_a;
		e_a = e_b;
		e_b = temp_e;
		swapped = true;
	}

	dx_shape s_a = shapes_srv[e_a.shape_index];
	dx_shape s_b = shapes_srv[e_b.shape_index];
	dx_collision_full c;
	if (collision_test_sphere_obb(e_a, s_a, e_b, s_b, c)) {
		write_collision(p, c, swapped);
	}
}

[Shader("node")]
[NodeLaunch("thread")]
void Narrow_Cap_Cap(ThreadNodeInputRecord<dx_potential_pair> input) {
	dx_potential_pair p = input.Get();
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) {
		e_b = entities_srv[p.b_index];
	} else {
		e_b = statics_srv[p.b_index];
	}

	bool swapped = false;
	if (e_a.shape_type > e_b.shape_type) {
		dx_entity temp_e = e_a;
		e_a = e_b;
		e_b = temp_e;
		swapped = true;
	}

	dx_shape s_a = shapes_srv[e_a.shape_index];
	dx_shape s_b = shapes_srv[e_b.shape_index];
	dx_collision_full c;
	if (collision_test_capsule_capsule(e_a, s_a, e_b, s_b, c)) {
		write_collision(p, c, swapped);
	}
}

[Shader("node")]
[NodeLaunch("thread")]
void Narrow_Cap_Box(ThreadNodeInputRecord<dx_potential_pair> input) {
	dx_potential_pair p = input.Get();
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) {
		e_b = entities_srv[p.b_index];
	} else {
		e_b = statics_srv[p.b_index];
	}

	bool swapped = false;
	if (e_a.shape_type > e_b.shape_type) {
		dx_entity temp_e = e_a;
		e_a = e_b;
		e_b = temp_e;
		swapped = true;
	}

	dx_shape s_a = shapes_srv[e_a.shape_index];
	dx_shape s_b = shapes_srv[e_b.shape_index];
	dx_collision_full c;
	if (collision_test_capsule_obb(e_a, s_a, e_b, s_b, c)) {
		write_collision(p, c, swapped);
	}
}

[Shader("node")]
[NodeLaunch("thread")]
void Narrow_Box_Box(ThreadNodeInputRecord<dx_potential_pair> input) {
	dx_potential_pair p = input.Get();
	dx_entity e_a = entities_srv[p.a_index];
	dx_entity e_b;
	if (p.b_type == 1) {
		e_b = entities_srv[p.b_index];
	} else {
		e_b = statics_srv[p.b_index];
	}

	bool swapped = false;
	if (e_a.shape_type > e_b.shape_type) {
		dx_entity temp_e = e_a;
		e_a = e_b;
		e_b = temp_e;
		swapped = true;
	}

	dx_shape s_a = shapes_srv[e_a.shape_index];
	dx_shape s_b = shapes_srv[e_b.shape_index];
	dx_collision_full c;
	if (collision_test_obb_obb(e_a, s_a, e_b, s_b, c)) {
		write_collision(p, c, swapped);
	}
}
