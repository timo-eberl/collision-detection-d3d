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

float4 quat_inverse(float4 q) {
	return float4(-q.x, -q.y, -q.z, q.w);
}

float3 world_to_local(float3 p, float3 pos, float4 rot) {
	return rotate_vector(p - pos, quat_inverse(rot));
}

float3 local_to_world(float3 p, float3 pos, float4 rot) {
	return pos + rotate_vector(p, rot);
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
                                  out dx_collision result) {
	result = (dx_collision)0;

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
                                   out dx_collision result) {
	result = (dx_collision)0;

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
									out dx_collision result) {
	result = (dx_collision)0;

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
                               out dx_collision result) {
	result = (dx_collision)0;

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
                                out dx_collision result) {
	result = (dx_collision)0;

	float3 up_a = rotate_vector(float3(0.0f, 1.0f, 0.0f), e_a.rotation);
	float3 world_a = e_a.position - up_a * s_a.data.x; // Corrected to -
	float3 world_b = e_a.position + up_a * s_a.data.x; // Corrected to +

	float3 A = world_to_local(world_a, e_b.position, e_b.rotation);
	float3 B = world_to_local(world_b, e_b.position, e_b.rotation);
	float3 D = B - A;

	float r = s_a.data.y;
	float3 ext = s_b.data.xyz;
	float3 exp_ext = ext + r;

	// Vectorized Ray-AABB intersection for the shallow penetration distance query.
	// We use a sign-preserving epsilon to avoid NaN generation on exact parallel zero-divisions.
	float3 abs_D = abs(D);
	float3 D_safe;
	D_safe.x = abs_D.x < 0.00001f ? (D.x >= 0.0f ? 0.00001f : -0.00001f) : D.x;
	D_safe.y = abs_D.y < 0.00001f ? (D.y >= 0.0f ? 0.00001f : -0.00001f) : D.y;
	D_safe.z = abs_D.z < 0.00001f ? (D.z >= 0.0f ? 0.00001f : -0.00001f) : D.z;

	float3 invD = 1.0f / D_safe;
	float3 t0 = (-exp_ext - A) * invD;
	float3 t1 = ( exp_ext - A) * invD;

	float3 t_min3 = min(t0, t1);
	float3 t_max3 = max(t0, t1);

	float t_min_ray = max(max(t_min3.x, t_min3.y), t_min3.z);
	float t_max_ray = min(min(t_max3.x, t_max3.y), t_max3.z);

	if (t_max_ray < t_min_ray || t_max_ray < 0.0f || t_min_ray > 1.0f) return false;

	float t_hit = clamp(t_min_ray, 0.0f, 1.0f);
	float3 P_entry = A + D * t_hit;

	bool out_x = abs(P_entry.x) > ext.x;
	bool out_y = abs(P_entry.y) > ext.y;
	bool out_z = abs(P_entry.z) > ext.z;
	int outside_count = (out_x ? 1 : 0) + (out_y ? 1 : 0) + (out_z ? 1 : 0);

	float3 P_seg = float3(0.0f, 0.0f, 0.0f);
	float3 Q_box = float3(0.0f, 0.0f, 0.0f);

	if (outside_count == 1) {
		if (out_x) {
			P_seg = (P_entry.x > 0.0f) ? ((A.x < B.x) ? A : B) : ((A.x > B.x) ? A : B);
		} else if (out_y) {
			P_seg = (P_entry.y > 0.0f) ? ((A.y < B.y) ? A : B) : ((A.y > B.y) ? A : B);
		} else {
			P_seg = (P_entry.z > 0.0f) ? ((A.z < B.z) ? A : B) : ((A.z > B.z) ? A : B);
		}
		Q_box = clamp(P_seg, -ext, ext);
	} else if (outside_count == 2) {
		float3 eA = float3(0.0f, 0.0f, 0.0f);
		float3 eB = float3(0.0f, 0.0f, 0.0f);
		if (!out_x) {
			eA = float3(-ext.x, P_entry.y > 0.0f ? ext.y : -ext.y, P_entry.z > 0.0f ? ext.z : -ext.z);
			eB = float3( ext.x, P_entry.y > 0.0f ? ext.y : -ext.y, P_entry.z > 0.0f ? ext.z : -ext.z);
		} else if (!out_y) {
			eA = float3(P_entry.x > 0.0f ? ext.x : -ext.x, -ext.y, P_entry.z > 0.0f ? ext.z : -ext.z);
			eB = float3(P_entry.x > 0.0f ? ext.x : -ext.x,  ext.y, P_entry.z > 0.0f ? ext.z : -ext.z);
		} else {
			eA = float3(P_entry.x > 0.0f ? ext.x : -ext.x, P_entry.y > 0.0f ? ext.y : -ext.y, -ext.z);
			eB = float3(P_entry.x > 0.0f ? ext.x : -ext.x, P_entry.y > 0.0f ? ext.y : -ext.y,  ext.z);
		}
		closest_points_between_segments(A, B, eA, eB, P_seg, Q_box);
	} else if (outside_count == 3) {
		float3 V = float3(
			P_entry.x > 0.0f ? ext.x : -ext.x,
			P_entry.y > 0.0f ? ext.y : -ext.y,
			P_entry.z > 0.0f ? ext.z : -ext.z
		);
		P_seg = closest_point_on_segment(V, A, B);
		Q_box = V;
	}

	float3 delta = P_seg - Q_box;
	float dist_sq = dot(delta, delta);

	if (dist_sq > r * r) return false;

	if (outside_count > 0 && dist_sq > 0.00001f) {
		float dist = sqrt(dist_sq);
		
		result.depth = r - dist;
		float3 local_normal = delta * (1.0f / dist);
		
		result.normal = rotate_vector(local_normal, e_b.rotation);
		result.point_b = local_to_world(Q_box, e_b.position, e_b.rotation);
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

bool collision_test_obb_obb(dx_entity e_a, dx_shape s_a, dx_entity e_b, dx_shape s_b, 
                            out dx_collision result) {
	result = (dx_collision)0;
	// TODO implement
	return false;
}

bool evaluate_narrow_phase(dx_entity e_a, dx_shape s_a, dx_entity e_b, dx_shape s_b, 
                           out dx_collision result) {
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

	dx_collision c;
	if (evaluate_narrow_phase(e_a, s_a, e_b, s_b, c)) {
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
