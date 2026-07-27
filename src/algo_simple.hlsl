struct dx_entity {
	float3 position;
	uint shape_type;
	float4 rotation;
	uint shape_index;
	uint3 pad;
};

struct dx_shape {
	float4 data;
};

struct dx_potential_pair {
	uint a_index;
	uint b_index;
	uint b_type;
	uint pad;
};

struct packed_aabb {
	float min_x;
	float max_x;
	float min_y;
	float max_y;
	float min_z;
	float max_z;
};

struct dx_collision {
	uint a_index;
	uint b_index;
	uint b_type;
	float depth;
	float3 point_a;
	float3 point_b;
	float3 normal;
	uint3 pad;
};

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
StructuredBuffer<dx_potential_pair> potential_pairs_srv : register(t5);

RWStructuredBuffer<packed_aabb> aabb_uav : register(u0);
RWStructuredBuffer<dx_potential_pair> potential_pairs_uav : register(u1);
RWStructuredBuffer<uint> pair_count_uav : register(u2);
RWStructuredBuffer<dx_collision> collisions_uav : register(u3);
RWStructuredBuffer<uint> col_count_uav : register(u4);

float3 rotate_vector(float3 v, float4 q) {
	float3 q_xyz = q.xyz;
	float3 t = 2.0f * cross(q_xyz, v);
	return v + q.w * t + cross(q_xyz, t);
}

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
		float3 p_a = e.position + up * half_height;
		float3 p_b = e.position - up * half_height;
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

bool aabb_overlap(packed_aabb a, packed_aabb b) {
	return a.max_x >= b.min_x && a.min_x <= b.max_x &&
	       a.max_y >= b.min_y && a.min_y <= b.max_y &&
	       a.max_z >= b.min_z && a.min_z <= b.max_z;
}

[numthreads(256, 1, 1)]
void cs_broad_phase(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= rigid_count) return;

	packed_aabb box_i = aabb_rigids_srv[i];

	for (uint j = i + 1; j < rigid_count; ++j) {
		if (aabb_overlap(box_i, aabb_rigids_srv[j])) {
			uint idx;
			InterlockedAdd(pair_count_uav[0], 1, idx);
			if (idx < max_collisions) {
				dx_potential_pair p;
				p.a_index = i;
				p.b_index = j;
				p.b_type = 1;
				p.pad = 0;
				potential_pairs_uav[idx] = p;
			}
		}
	}

	for (uint k = 0; k < static_count; ++k) {
		if (aabb_overlap(box_i, aabb_statics_srv[k])) {
			uint idx;
			InterlockedAdd(pair_count_uav[0], 1, idx);
			if (idx < max_collisions) {
				dx_potential_pair p;
				p.a_index = i;
				p.b_index = k;
				p.b_type = 0;
				p.pad = 0;
				potential_pairs_uav[idx] = p;
			}
		}
	}
}

float3 closest_point_on_segment(float3 p, float3 a, float3 b) {
	float3 ab = b - a;
	float len_sq = dot(ab, ab);

	if (len_sq <= 0.00001f) return a;

	float t = saturate(dot(p - a, ab) / len_sq);
	return a + ab * t;
}

void closest_points_between_segments(float3 p1, float3 q1, float3 p2, float3 q2, 
                                     out float3 c1, out float3 c2) {
	float3 d1 = q1 - p1;
	float3 d2 = q2 - p2;
	float3 r = p1 - p2;

	float a = dot(d1, d1);
	float e = dot(d2, d2);
	float f = dot(d2, r);

	if (a <= 0.00001f && e <= 0.00001f) {
		c1 = p1;
		c2 = p2;
		return;
	}

	float s = 0.0f;
	float t = 0.0f;

	if (a <= 0.00001f) {
		t = saturate(f / e);
	} else {
		float c = dot(d1, r);
		if (e <= 0.00001f) {
			s = saturate(-c / a);
		} else {
			float b = dot(d1, d2);
			float denom = a * e - b * b;

			if (denom != 0.0f) {
				s = saturate((b * f - c * e) / denom);
			}

			t = (b * s + f) / e;

			if (t < 0.0f) {
				t = 0.0f;
				s = saturate(-c / a);
			} else if (t > 1.0f) {
				t = 1.0f;
				s = saturate((b - c) / a);
			}
		}
	}

	c1 = p1 + d1 * s;
	c2 = p2 + d2 * t;
}

struct col_shape {
	float3 p_a;
	float radius;
	float3 p_b;
	uint type;
};

col_shape get_col_shape(dx_entity e, dx_shape s) {
	col_shape cs;
	cs.type = e.shape_type;
	if (e.shape_type == 0) {
		cs.p_a = e.position;
		cs.radius = s.data.x;
		cs.p_b = e.position;
	} else if (e.shape_type == 1) {
		float3 up = rotate_vector(float3(0.0f, 1.0f, 0.0f), e.rotation);
		cs.p_a = e.position + up * s.data.x;
		cs.p_b = e.position - up * s.data.x;
		cs.radius = s.data.y;
	} else {
		cs.p_a = e.position;
		cs.p_b = e.position;
		cs.radius = 0.0f;
	}
	return cs;
}

bool collision_test_sphere_sphere(col_shape a, col_shape b, out dx_collision result) {
	result = (dx_collision)0;

	float radius_sum = a.radius + b.radius;
	float3 delta = b.p_a - a.p_a;
	float dist_sq = dot(delta, delta);

	if (dist_sq > radius_sum * radius_sum) return false;

	float distance = sqrt(dist_sq);
	if (distance < 0.0001f) {
		result.depth = radius_sum;
		result.normal = float3(0.0f, 1.0f, 0.0f);
	} else {
		result.depth = radius_sum - distance;
		result.normal = delta * (-1.0f / distance);
	}

	result.point_a = a.p_a + result.normal * -a.radius;
	result.point_b = b.p_a + result.normal * b.radius;

	return true;
}

bool collision_test_sphere_capsule(col_shape a, col_shape b, out dx_collision result) {
	result = (dx_collision)0;

	float3 closest_on_cap = closest_point_on_segment(a.p_a, b.p_a, b.p_b);

	float radius_sum = a.radius + b.radius;
	float3 delta = closest_on_cap - a.p_a;
	float dist_sq = dot(delta, delta);

	if (dist_sq > radius_sum * radius_sum) return false;

	float distance = sqrt(dist_sq);
	if (distance < 0.0001f) {
		result.depth = radius_sum;
		result.normal = float3(0.0f, 1.0f, 0.0f);
	} else {
		result.depth = radius_sum - distance;
		result.normal = delta * (-1.0f / distance);
	}

	result.point_a = a.p_a + result.normal * -a.radius;
	result.point_b = closest_on_cap + result.normal * b.radius;

	return true;
}

bool collision_test_capsule_capsule(col_shape a, col_shape b, out dx_collision result) {
	result = (dx_collision)0;

	float3 closest_a, closest_b;
	closest_points_between_segments(a.p_a, a.p_b, b.p_a, b.p_b, closest_a, closest_b);

	float radius_sum = a.radius + b.radius;
	float3 delta = closest_b - closest_a;
	float dist_sq = dot(delta, delta);

	if (dist_sq > radius_sum * radius_sum) return false;

	float distance = sqrt(dist_sq);
	if (distance < 0.0001f) {
		result.depth = radius_sum;
		result.normal = float3(0.0f, 1.0f, 0.0f);
	} else {
		result.depth = radius_sum - distance;
		result.normal = delta * (-1.0f / distance);
	}

	result.point_a = closest_a + result.normal * -a.radius;
	result.point_b = closest_b + result.normal * b.radius;

	return true;
}

bool collision_test_sphere_obb(col_shape a, col_shape b, out dx_collision result) {
	result = (dx_collision)0;
	// TODO implement
	return false;
}

bool collision_test_capsule_obb(col_shape a, col_shape b, out dx_collision result) {
	result = (dx_collision)0;
	// TODO implement
	return false;
}

bool collision_test_obb_obb(col_shape a, col_shape b, out dx_collision result) {
	result = (dx_collision)0;
	// TODO implement
	return false;
}

bool evaluate_narrow_phase(col_shape a, col_shape b, out dx_collision result) {
	bool swapped = false;
	
	if (a.type > b.type) {
		col_shape temp_shape = a;
		a = b;
		b = temp_shape;
		swapped = true;
	}

	bool hit = false;
	if (a.type == 0 && b.type == 0) hit = collision_test_sphere_sphere(a, b, result);
	else if (a.type == 0 && b.type == 1) hit = collision_test_sphere_capsule(a, b, result);
	else if (a.type == 1 && b.type == 1) hit = collision_test_capsule_capsule(a, b, result);
	else if (a.type == 0 && b.type == 2) hit = collision_test_sphere_obb(a, b, result);
	else if (a.type == 1 && b.type == 2) hit = collision_test_capsule_obb(a, b, result);
	else if (a.type == 2 && b.type == 2) hit = collision_test_obb_obb(a, b, result);

	if (swapped && hit) {
		result.normal = -result.normal;
		float3 temp_pt = result.point_a;
		result.point_a = result.point_b;
		result.point_b = temp_pt;
	}

	return hit;
}

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

	col_shape cs_a = get_col_shape(e_a, s_a);
	col_shape cs_b = get_col_shape(e_b, s_b);

	dx_collision c;
	if (evaluate_narrow_phase(cs_a, cs_b, c)) {
		uint idx;
		InterlockedAdd(col_count_uav[0], 1, idx);
		if (idx < max_collisions) {
			c.a_index = p.a_index;
			c.b_index = p.b_index;
			c.b_type = p.b_type;
			collisions_uav[idx] = c;
		}
	}
}
