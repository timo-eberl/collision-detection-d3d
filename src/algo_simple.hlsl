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

struct dx_collision_full {
	uint a_index;
	uint b_index;
	uint b_type;
	float depth;
	float3 point_a;
	float3 point_b;
	float3 normal;
	uint3 pad;
};

struct dx_collision_compact {
	uint a_index;
	uint b_index;
	float depth;
	float3 point_a;
	float2 normal;
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
RWStructuredBuffer<dx_collision_compact> collisions_uav : register(u3);
RWStructuredBuffer<uint> col_count_uav : register(u4);

float3 rotate_vector(float3 v, float4 q) {
	float3 q_xyz = q.xyz;
	float3 t = 2.0f * cross(q_xyz, v);
	return v + q.w * t + cross(q_xyz, t);
}

float4 quat_inverse(float4 q) {
	return float4(-q.x, -q.y, -q.z, q.w);
}

// Calculates the Grassman product (standard quaternion multiplication)
float4 quat_mul(float4 a, float4 b) {
	return float4(
		a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
		a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
		a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
		a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z
	);
}

float3 world_to_local(float3 p, float3 pos, float4 rot) {
	return rotate_vector(p - pos, quat_inverse(rot));
}

float3 local_to_world(float3 p, float3 pos, float4 rot) {
	return pos + rotate_vector(p, rot);
}

float2 sign_not_zero(float2 v) {
	return float2((v.x >= 0.0f) ? 1.0f : -1.0f, (v.y >= 0.0f) ? 1.0f : -1.0f);
}

// Projects the sphere onto the octahedron, and then onto the xy plane
float2 encode_octahedral(float3 v) {
	float2 p = v.xy * (1.0f / (abs(v.x) + abs(v.y) + abs(v.z)));
	// Reflect the folds of the lower hemisphere over the diagonals
	return (v.z <= 0.0f) ? ((1.0f - abs(p.yx)) * sign_not_zero(p)) : p;
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
		float3 p_a = e.position - up * half_height; // -up is p_a
		float3 p_b = e.position + up * half_height; // +up is p_b
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

bool collision_test_sphere_sphere(dx_entity e_a, dx_shape s_a, dx_entity e_b, dx_shape s_b, 
                                  out dx_collision_full result) {
	result = (dx_collision_full)0;

	float radius_a = s_a.data.x;
	float radius_b = s_b.data.x;
	float radius_sum = radius_a + radius_b;
	
	float3 delta = e_b.position - e_a.position;
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

	result.point_a = e_a.position + result.normal * -radius_a;
	result.point_b = e_b.position + result.normal * radius_b;

	return true;
}

bool collision_test_sphere_capsule(dx_entity e_a, dx_shape s_a, dx_entity e_b, dx_shape s_b, 
                                   out dx_collision_full result) {
	result = (dx_collision_full)0;

	float radius_a = s_a.data.x;
	float radius_b = s_b.data.y;
	float radius_sum = radius_a + radius_b;

	float3 up_b = rotate_vector(float3(0.0f, 1.0f, 0.0f), e_b.rotation);
	float3 cap_p_a = e_b.position - up_b * s_b.data.x;
	float3 cap_p_b = e_b.position + up_b * s_b.data.x;

	float3 closest_on_cap = closest_point_on_segment(e_a.position, cap_p_a, cap_p_b);

	float3 delta = closest_on_cap - e_a.position;
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

	result.point_a = e_a.position + result.normal * -radius_a;
	result.point_b = closest_on_cap + result.normal * radius_b;

	return true;
}

bool collision_test_capsule_capsule(dx_entity e_a, dx_shape s_a, dx_entity e_b, dx_shape s_b,
									out dx_collision_full result) {
	result = (dx_collision_full)0;

	float half_height_a = s_a.data.x;
	float radius_a = s_a.data.y;
	float half_height_b = s_b.data.x;
	float radius_b = s_b.data.y;

	float3 up_a = rotate_vector(float3(0.0f, 1.0f, 0.0f), e_a.rotation);
	float3 a_p_a = e_a.position - up_a * half_height_a;
	float3 a_p_b = e_a.position + up_a * half_height_a;

	float3 up_b = rotate_vector(float3(0.0f, 1.0f, 0.0f), e_b.rotation);
	float3 b_p_a = e_b.position - up_b * half_height_b;
	float3 b_p_b = e_b.position + up_b * half_height_b;

	float3 closest_a, closest_b;
	closest_points_between_segments(a_p_a, a_p_b, b_p_a, b_p_b, closest_a, closest_b);

	float radius_sum = radius_a + radius_b;
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

	result.point_a = closest_a + result.normal * -radius_a;
	result.point_b = closest_b + result.normal * radius_b;

	return true;
}

bool collision_test_sphere_obb(dx_entity e_a, dx_shape s_a, dx_entity e_b, dx_shape s_b, 
                               out dx_collision_full result) {
	result = (dx_collision_full)0;

	float radius = s_a.data.x;
	float3 extents = s_b.data.xyz;

	// Transform the sphere's center into the OBB's local coordinate space to simplify the OBB
	// into an AABB centered at the origin
	float3 local_center = world_to_local(e_a.position, e_b.position, e_b.rotation);

	// Find the closest point on the AABB to the sphere center by clamping the coordinates
	float3 clamped = clamp(local_center, -extents, extents);
	float3 delta = local_center - clamped;
	float dist_sq = dot(delta, delta);

	float3 local_normal;
	float depth;

	// If the sphere's center is inside the box, the clamped point equals the center point.
	// This results in a zero distance vector, which cannot be normalized to find a push-out
	// direction. To resolve this deep penetration, we calculate the distance from the center
	// to each of the 3 geometric faces and force the collision onto the closest one.
	if (dist_sq < 0.00001f) {
		float3 dist_to_face = extents - abs(local_center);

		if (dist_to_face.x <= dist_to_face.y && dist_to_face.x <= dist_to_face.z) {
			clamped.x = local_center.x > 0.0f ? extents.x : -extents.x;
			local_normal = float3(local_center.x > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
			depth = radius + dist_to_face.x;
		} else if (dist_to_face.y <= dist_to_face.x && dist_to_face.y <= dist_to_face.z) {
			clamped.y = local_center.y > 0.0f ? extents.y : -extents.y;
			local_normal = float3(0.0f, local_center.y > 0.0f ? 1.0f : -1.0f, 0.0f);
			depth = radius + dist_to_face.y;
		} else {
			clamped.z = local_center.z > 0.0f ? extents.z : -extents.z;
			local_normal = float3(0.0f, 0.0f, local_center.z > 0.0f ? 1.0f : -1.0f);
			depth = radius + dist_to_face.z;
		}
	} else {
		if (dist_sq > radius * radius) return false;

		float dist = sqrt(dist_sq);
		depth = radius - dist;
		local_normal = delta / dist;
	}

	result.depth = depth;

	// Convert local calculations back into world space
	result.normal = rotate_vector(local_normal, e_b.rotation);
	result.point_b = local_to_world(clamped, e_b.position, e_b.rotation);
	
	// The deepest penetrating point on the sphere lies opposite to the collision normal
	result.point_a = e_a.position + result.normal * -radius;

	return true;
}

float3 get_box_support_point(float3 ext, float3 pos, float4 rot, float3 dir) {
	float3 local_dir = rotate_vector(dir, quat_inverse(rot));

	float3 local_support = float3(
		local_dir.x > 0.0f ? ext.x : -ext.x,
		local_dir.y > 0.0f ? ext.y : -ext.y,
		local_dir.z > 0.0f ? ext.z : -ext.z
	);

	return local_to_world(local_support, pos, rot);
}

void get_box_support_edge(float3 ext, float3 pos, float4 rot, float3 dir, 
                          out float3 p1, out float3 p2) {
	float3 local_dir = rotate_vector(dir, quat_inverse(rot));

	float3 local_support = float3(
		local_dir.x > 0.0f ? ext.x : -ext.x,
		local_dir.y > 0.0f ? ext.y : -ext.y,
		local_dir.z > 0.0f ? ext.z : -ext.z
	);

	float abs_x = abs(local_dir.x);
	float abs_y = abs(local_dir.y);
	float abs_z = abs(local_dir.z);

	float3 edge_start = local_support;
	float3 edge_end = local_support;

	// The supporting edge is formed along the local axis most perpendicular to the search direction
	if (abs_x <= abs_y && abs_x <= abs_z) {
		edge_start.x = -ext.x;
		edge_end.x = ext.x;
	} else if (abs_y <= abs_x && abs_y <= abs_z) {
		edge_start.y = -ext.y;
		edge_end.y = ext.y;
	} else {
		edge_start.z = -ext.z;
		edge_end.z = ext.z;
	}

	p1 = local_to_world(edge_start, pos, rot);
	p2 = local_to_world(edge_end, pos, rot);
}

bool test_capsule_box_axis(float3 axis, float3 ext, float3 A, float3 B, float r, 
                           inout float min_overlap, inout float3 best_axis, int type, 
                           inout int best_type) {
	float len_sq = dot(axis, axis);
	if (len_sq < 0.00001f) return true;

	float len = sqrt(len_sq);
	float3 n = axis * (1.0f / len);

	float r_box = ext.x * abs(n.x) + ext.y * abs(n.y) + ext.z * abs(n.z);

	float pA = dot(A, n);
	float pB = dot(B, n);
	float min_cap = min(pA, pB) - r;
	float max_cap = max(pA, pB) + r;

	if (min_cap > r_box || max_cap < -r_box) return false;

	float d1 = r_box - min_cap;
	float d2 = max_cap + r_box;

	float overlap;
	float3 push_n;
	if (d1 < d2) {
		overlap = d1;
		push_n = n;
	} else {
		overlap = d2;
		push_n = -n;
	}

	if (overlap < min_overlap) {
		// Face bias to prevent jittering when resting flat against a box face
		if (best_type == 0 && type == 1 && overlap > min_overlap * 0.999f) return true;

		min_overlap = overlap;
		best_axis = push_n;
		best_type = type;
	}
	return true;
}

bool collision_test_capsule_obb(dx_entity e_a, dx_shape s_a, dx_entity e_b, dx_shape s_b, 
                                out dx_collision_full result) {
	result = (dx_collision_full)0;

	float3 up_a = rotate_vector(float3(0.0f, 1.0f, 0.0f), e_a.rotation);
	float3 world_a = e_a.position - up_a * s_a.data.x; 
	float3 world_b = e_a.position + up_a * s_a.data.x; 

	float3 A = world_to_local(world_a, e_b.position, e_b.rotation);
	float3 B = world_to_local(world_b, e_b.position, e_b.rotation);
	float3 D = B - A;

	float r = s_a.data.y;
	float3 ext = s_b.data.xyz;
	float3 exp_ext = ext + r;

	float t_min_ray = 0.0f;
	float t_max_ray = 1.0f;
	
	float E_arr[3] = {exp_ext.x, exp_ext.y, exp_ext.z};
	float A_arr[3] = {A.x, A.y, A.z};
	float D_arr[3] = {D.x, D.y, D.z};

	[unroll]
	for (int i = 0; i < 3; i++) {
		if (abs(D_arr[i]) < 0.00001f) {
			if (A_arr[i] < -E_arr[i] || A_arr[i] > E_arr[i]) return false;
		} else {
			float invD = 1.0f / D_arr[i];
			float t0 = (-E_arr[i] - A_arr[i]) * invD;
			float t1 = ( E_arr[i] - A_arr[i]) * invD;
			if (invD < 0.0f) { float tmp = t0; t0 = t1; t1 = tmp; }
			if (t0 > t_min_ray) t_min_ray = t0;
			if (t1 < t_max_ray) t_max_ray = t1;
			if (t_max_ray < t_min_ray) return false;
		}
	}

	// We must find the exact closest point between the capsule core segment and the unexpanded Box.
	// Since the distance between a line segment and an AABB is a strictly convex function, we use
	// a Golden Section Search to robustly find the global minimum distance in exactly 32 iterations
	// without any branching on complex Voronoi regions.
	float t0 = max(0.0f, t_min_ray);
	float t1 = min(1.0f, t_max_ray);

	const float inv_phi = 0.6180339887f;
	const float inv_phi_2 = 0.3819660113f;

	float t_a = t0 + inv_phi_2 * (t1 - t0);
	float t_b = t0 + inv_phi * (t1 - t0);

	float3 pos_a = A + D * t_a;
	float3 proj_a = clamp(pos_a, -ext, ext);
	float3 d_a = pos_a - proj_a;
	float dist_sq_a = dot(d_a, d_a);

	float3 pos_b = A + D * t_b;
	float3 proj_b = clamp(pos_b, -ext, ext);
	float3 d_b = pos_b - proj_b;
	float dist_sq_b = dot(d_b, d_b);

	// The HLSL compiler will flatten this loop interior into branchless conditional moves (csel)
	// since both branches execute the exact same number and type of ALU operations.
	for (int iter = 0; iter < 32; iter++) {
		if (dist_sq_a < dist_sq_b) {
			t1 = t_b;
			t_b = t_a;
			dist_sq_b = dist_sq_a;
			t_a = t0 + inv_phi_2 * (t1 - t0);
			pos_a = A + D * t_a;
			proj_a = clamp(pos_a, -ext, ext);
			float3 temp_d = pos_a - proj_a;
			dist_sq_a = dot(temp_d, temp_d);
		} else {
			t0 = t_a;
			t_a = t_b;
			dist_sq_a = dist_sq_b;
			t_b = t0 + inv_phi * (t1 - t0);
			pos_b = A + D * t_b;
			proj_b = clamp(pos_b, -ext, ext);
			float3 temp_d = pos_b - proj_b;
			dist_sq_b = dot(temp_d, temp_d);
		}
	}

	float best_t = (t0 + t1) * 0.5f;
	float3 p_seg = A + D * best_t;
	float3 q_box = clamp(p_seg, -ext, ext);

	float3 delta = p_seg - q_box;
	float dist_sq = dot(delta, delta);

	if (dist_sq > r * r) return false;

	// If distance is near zero, the core segment has breached the interior of the unexpanded Box.
	// We skip shallow resolution and fall back to SAT below.
	if (dist_sq > 0.00001f) {
		float dist = sqrt(dist_sq);
		
		result.depth = r - dist;
		float3 local_normal = delta * (1.0f / dist);
		
		result.normal = rotate_vector(local_normal, e_b.rotation);
		result.point_b = local_to_world(q_box, e_b.position, e_b.rotation);
		result.point_a = result.point_b - result.normal * result.depth;
		
		return true;
	}

	float min_overlap = 3.402823466e+38f;
	float3 best_axis = float3(0.0f, 0.0f, 0.0f);
	int best_type = -1;

	if (!test_capsule_box_axis(float3(1.0f, 0.0f, 0.0f), ext, A, B, r, min_overlap, best_axis, 0, best_type)) return false;
	if (!test_capsule_box_axis(float3(0.0f, 1.0f, 0.0f), ext, A, B, r, min_overlap, best_axis, 0, best_type)) return false;
	if (!test_capsule_box_axis(float3(0.0f, 0.0f, 1.0f), ext, A, B, r, min_overlap, best_axis, 0, best_type)) return false;

	if (!test_capsule_box_axis(float3(0.0f, -D.z, D.y), ext, A, B, r, min_overlap, best_axis, 1, best_type)) return false;
	if (!test_capsule_box_axis(float3(D.z, 0.0f, -D.x), ext, A, B, r, min_overlap, best_axis, 1, best_type)) return false;
	if (!test_capsule_box_axis(float3(-D.y, D.x, 0.0f), ext, A, B, r, min_overlap, best_axis, 1, best_type)) return false;

	result.depth = min_overlap;
	result.normal = rotate_vector(best_axis, e_b.rotation);

	float dot_a = dot(world_a, result.normal);
	float dot_b = dot(world_b, result.normal);
	float3 deepest_core = (dot_a < dot_b) ? world_a : world_b;

	if (best_type == 0) {
		result.point_a = deepest_core + result.normal * -r;
	} else {
		float3 box_p1, box_p2;
		get_box_support_edge(ext, e_b.position, e_b.rotation, result.normal, box_p1, box_p2);

		float3 core_a;
		closest_points_between_segments(world_a, world_b, box_p1, box_p2, core_a, result.point_b);
		result.point_a = core_a + result.normal * -r;
	}

	result.point_b = result.point_a + result.normal * result.depth;

	return true;
}

bool test_edge_axis(float3 axis, float r_a, float r_b, float abs_t, 
                    inout float min_overlap, inout float3 best_axis, inout int best_type) {
	float len_sq = dot(axis, axis);
	
	// Safely skip degenerate axes where the cross product length is near zero (parallel edges)
	if (len_sq > 0.00001f) {
		float len = sqrt(len_sq);
		float overlap = (r_a + r_b - abs_t) / len;

		if (overlap < 0.0f) return false;

		if (overlap < min_overlap) {
			// Bias towards Face-Axes (type 0 or 1) to prevent jitter from floating-point
			// inaccuracies during flat face-to-face contact
			if (best_type < 2 && overlap > min_overlap * 0.999f) {
				return true;
			}
			min_overlap = overlap;
			best_axis = axis * (1.0f / len);
			best_type = 2;
		}
	}
	return true;
}

bool collision_test_obb_obb(dx_entity e_a, dx_shape s_a, dx_entity e_b, dx_shape s_b, 
                            out dx_collision_full result) {
	result = (dx_collision_full)0;

	// B's position in A's local coordinate space
	float3 T = world_to_local(e_b.position, e_a.position, e_a.rotation);

	// Relative rotation quaternion from A to B
	float4 rel_q = quat_mul(quat_inverse(e_a.rotation), e_b.rotation);
	
	// Extract the local axes of Box B in Box A's coordinate space.
	// We manually expand the quaternion to a 3x3 matrix to perfectly match the 
	// CPU's float-math operations and prevent precision-based SAT axis drift.
	float xx = rel_q.x * rel_q.x, yy = rel_q.y * rel_q.y, zz = rel_q.z * rel_q.z;
	float xy = rel_q.x * rel_q.y, xz = rel_q.x * rel_q.z, yz = rel_q.y * rel_q.z;
	float wx = rel_q.w * rel_q.x, wy = rel_q.w * rel_q.y, wz = rel_q.w * rel_q.z;

	float3 r_x = float3(1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz),       2.0f * (xz - wy));
	float3 r_y = float3(2.0f * (xy - wz),       1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx));
	float3 r_z = float3(2.0f * (xz + wy),       2.0f * (yz - wx),       1.0f - 2.0f * (xx + yy));

	// Calculate absolute matrices with an epsilon to prevent division-by-zero or zero-length
	// vectors during edge-edge cross products for perfectly axis-aligned boxes
	float3 abs_rx = abs(r_x) + 0.000001f;
	float3 abs_ry = abs(r_y) + 0.000001f;
	float3 abs_rz = abs(r_z) + 0.000001f;

	float3 eA = s_a.data.xyz;
	float3 eB = s_b.data.xyz;

	float min_overlap = 3.402823466e+38f;
	float3 best_axis = float3(0.0f, 0.0f, 0.0f);
	int best_type = -1; // 0 for Face A, 1 for Face B, 2 for Edge-Edge
	float ra, rb, overlap;

	// --- 3 Face Axes of Box A ---
	// Axis X
	ra = eA.x;
	rb = dot(eB, float3(abs_rx.x, abs_ry.x, abs_rz.x));
	overlap = ra + rb - abs(T.x);
	if (overlap < 0.0f) return false;
	min_overlap = overlap; best_axis = float3(1.0f, 0.0f, 0.0f); best_type = 0;

	// Axis Y
	ra = eA.y;
	rb = dot(eB, float3(abs_rx.y, abs_ry.y, abs_rz.y));
	overlap = ra + rb - abs(T.y);
	if (overlap < 0.0f) return false;
	if (overlap < min_overlap) { min_overlap = overlap; best_axis = float3(0.0f, 1.0f, 0.0f); best_type = 0; }

	// Axis Z
	ra = eA.z;
	rb = dot(eB, float3(abs_rx.z, abs_ry.z, abs_rz.z));
	overlap = ra + rb - abs(T.z);
	if (overlap < 0.0f) return false;
	if (overlap < min_overlap) { min_overlap = overlap; best_axis = float3(0.0f, 0.0f, 1.0f); best_type = 0; }

	// --- 3 Face Axes of Box B ---
	// Axis X
	ra = dot(eA, abs_rx);
	rb = eB.x;
	overlap = ra + rb - abs(dot(T, r_x));
	if (overlap < 0.0f) return false;
	if (overlap < min_overlap) { min_overlap = overlap; best_axis = r_x; best_type = 1; }

	// Axis Y
	ra = dot(eA, abs_ry);
	rb = eB.y;
	overlap = ra + rb - abs(dot(T, r_y));
	if (overlap < 0.0f) return false;
	if (overlap < min_overlap) { min_overlap = overlap; best_axis = r_y; best_type = 1; }

	// Axis Z
	ra = dot(eA, abs_rz);
	rb = eB.z;
	overlap = ra + rb - abs(dot(T, r_z));
	if (overlap < 0.0f) return false;
	if (overlap < min_overlap) { min_overlap = overlap; best_axis = r_z; best_type = 1; }

	// --- 9 Edge-Edge Axes ---
	// Ax x Bx
	if (!test_edge_axis(float3(0.0f, -r_x.z, r_x.y),
	                    eA.y * abs_rx.z + eA.z * abs_rx.y,
	                    eB.y * abs_rz.x + eB.z * abs_ry.x,
	                    abs(T.y * r_x.z - T.z * r_x.y),
	                    min_overlap, best_axis, best_type)) return false;

	// Ax x By
	if (!test_edge_axis(float3(0.0f, -r_y.z, r_y.y),
	                    eA.y * abs_ry.z + eA.z * abs_ry.y,
	                    eB.x * abs_rz.x + eB.z * abs_rx.x,
	                    abs(T.y * r_y.z - T.z * r_y.y),
	                    min_overlap, best_axis, best_type)) return false;

	// Ax x Bz
	if (!test_edge_axis(float3(0.0f, -r_z.z, r_z.y),
	                    eA.y * abs_rz.z + eA.z * abs_rz.y,
	                    eB.x * abs_ry.x + eB.y * abs_rx.x,
	                    abs(T.y * r_z.z - T.z * r_z.y),
	                    min_overlap, best_axis, best_type)) return false;

	// Ay x Bx
	if (!test_edge_axis(float3(r_x.z, 0.0f, -r_x.x),
	                    eA.x * abs_rx.z + eA.z * abs_rx.x,
	                    eB.y * abs_rz.y + eB.z * abs_ry.y,
	                    abs(T.z * r_x.x - T.x * r_x.z),
	                    min_overlap, best_axis, best_type)) return false;

	// Ay x By
	if (!test_edge_axis(float3(r_y.z, 0.0f, -r_y.x),
	                    eA.x * abs_ry.z + eA.z * abs_ry.x,
	                    eB.x * abs_rz.y + eB.z * abs_rx.y,
	                    abs(T.z * r_y.x - T.x * r_y.z),
	                    min_overlap, best_axis, best_type)) return false;

	// Ay x Bz
	if (!test_edge_axis(float3(r_z.z, 0.0f, -r_z.x),
	                    eA.x * abs_rz.z + eA.z * abs_rz.x,
	                    eB.x * abs_ry.y + eB.y * abs_rx.y,
	                    abs(T.z * r_z.x - T.x * r_z.z),
	                    min_overlap, best_axis, best_type)) return false;

	// Az x Bx
	if (!test_edge_axis(float3(-r_x.y, r_x.x, 0.0f),
	                    eA.x * abs_rx.y + eA.y * abs_rx.x,
	                    eB.y * abs_rz.z + eB.z * abs_ry.z,
	                    abs(T.x * r_x.y - T.y * r_x.x),
	                    min_overlap, best_axis, best_type)) return false;

	// Az x By
	if (!test_edge_axis(float3(-r_y.y, r_y.x, 0.0f),
	                    eA.x * abs_ry.y + eA.y * abs_ry.x,
	                    eB.x * abs_rz.z + eB.z * abs_rx.z,
	                    abs(T.x * r_y.y - T.y * r_y.x),
	                    min_overlap, best_axis, best_type)) return false;

	// Az x Bz
	if (!test_edge_axis(float3(-r_z.y, r_z.x, 0.0f),
	                    eA.x * abs_rz.y + eA.y * abs_rz.x,
	                    eB.x * abs_ry.z + eB.y * abs_rx.z,
	                    abs(T.x * r_z.y - T.y * r_z.x),
	                    min_overlap, best_axis, best_type)) return false;

	if (min_overlap <= 0.0f) return false;

	result.depth = min_overlap;

	// Enforce convention: Normal must strictly point from Shape B to Shape A
	if (dot(best_axis, T) > 0.0f) {
		best_axis = -best_axis;
	}

	result.normal = rotate_vector(best_axis, e_a.rotation);

	if (best_type == 0) {
		result.point_b = get_box_support_point(eB, e_b.position, e_b.rotation, result.normal);
		result.point_a = result.point_b - result.normal * result.depth;
	} else if (best_type == 1) {
		result.point_a = get_box_support_point(eA, e_a.position, e_a.rotation, -result.normal);
		result.point_b = result.point_a + result.normal * result.depth;
	} else {
		float3 a_p1, a_p2, b_p1, b_p2;
		get_box_support_edge(eA, e_a.position, e_a.rotation, -result.normal, a_p1, a_p2);
		get_box_support_edge(eB, e_b.position, e_b.rotation, result.normal, b_p1, b_p2);

		closest_points_between_segments(a_p1, a_p2, b_p1, b_p2, result.point_a, result.point_b);
	}

	return true;
}

bool evaluate_narrow_phase(dx_entity e_a, dx_shape s_a, dx_entity e_b, dx_shape s_b, 
                           out dx_collision_full result) {
	bool swapped = false;
	
	if (e_a.shape_type > e_b.shape_type) {
		dx_entity temp_e = e_a;
		e_a = e_b;
		e_b = temp_e;

		dx_shape temp_s = s_a;
		s_a = s_b;
		s_b = temp_s;
		
		swapped = true;
	}

	bool hit = false;
	if (e_a.shape_type == 0 && e_b.shape_type == 0) {
		hit = collision_test_sphere_sphere(e_a, s_a, e_b, s_b, result);
	} else if (e_a.shape_type == 0 && e_b.shape_type == 1) {
		hit = collision_test_sphere_capsule(e_a, s_a, e_b, s_b, result);
	} else if (e_a.shape_type == 1 && e_b.shape_type == 1) {
		hit = collision_test_capsule_capsule(e_a, s_a, e_b, s_b, result);
	} else if (e_a.shape_type == 0 && e_b.shape_type == 2) {
		hit = collision_test_sphere_obb(e_a, s_a, e_b, s_b, result);
	} else if (e_a.shape_type == 1 && e_b.shape_type == 2) {
		hit = collision_test_capsule_obb(e_a, s_a, e_b, s_b, result);
	} else if (e_a.shape_type == 2 && e_b.shape_type == 2) {
		hit = collision_test_obb_obb(e_a, s_a, e_b, s_b, result);
	}

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
