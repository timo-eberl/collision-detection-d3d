struct dx_entity {
	float3 position;
	uint shape_info;
	float4 rotation;
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
	uint shape_type;
	uint pad;
};

struct dx_collision_compact {
	uint a_index;
	uint b_index;
	float depth;
	float3 point_a;
	float2 normal;
};

float3 rotate_vector(float3 v, float4 q) {
	float3 q_xyz = q.xyz;
	float3 t = 2.0f * cross(q_xyz, v);
	return v + q.w * t + cross(q_xyz, t);
}

float4 quat_inverse(float4 q) {
	return float4(-q.x, -q.y, -q.z, q.w);
}

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

float2 encode_octahedral(float3 v) {
	float2 p = v.xy * (1.0f / (abs(v.x) + abs(v.y) + abs(v.z)));
	return (v.z <= 0.0f) ? ((1.0f - abs(p.yx)) * sign_not_zero(p)) : p;
}

bool aabb_overlap(packed_aabb a, packed_aabb b) {
	return a.max_x >= b.min_x && a.min_x <= b.max_x &&
	       a.max_y >= b.min_y && a.min_y <= b.max_y &&
	       a.max_z >= b.min_z && a.min_z <= b.max_z;
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

bool collision_test_sphere_sphere(float3 pos_a, float radius_a, float3 pos_b, float radius_b,
								  out float depth, out float3 normal, out float3 point_a,
								  out float3 point_b) {
	depth = 0.0f;
	normal = float3(0.0f, 0.0f, 0.0f);
	point_a = float3(0.0f, 0.0f, 0.0f);
	point_b = float3(0.0f, 0.0f, 0.0f);

	float radius_sum = radius_a + radius_b;
	float3 delta = pos_b - pos_a;
	float dist_sq = dot(delta, delta);

	if (dist_sq > radius_sum * radius_sum) return false;

	float distance = sqrt(dist_sq);
	if (distance < 0.0001f) {
		depth = radius_sum;
		normal = float3(0.0f, 1.0f, 0.0f);
	} else {
		depth = radius_sum - distance;
		normal = delta * (-1.0f / distance);
	}

	point_a = pos_a + normal * -radius_a;
	point_b = pos_b + normal * radius_b;

	return true;
}

bool collision_test_sphere_capsule(float3 pos_a, float radius_a, float3 pos_b, float4 rot_b,
								   float half_height_b, float radius_b, out float depth,
								   out float3 normal, out float3 point_a, out float3 point_b) {
	depth = 0.0f;
	normal = float3(0.0f, 0.0f, 0.0f);
	point_a = float3(0.0f, 0.0f, 0.0f);
	point_b = float3(0.0f, 0.0f, 0.0f);

	float radius_sum = radius_a + radius_b;

	float3 up_b = rotate_vector(float3(0.0f, 1.0f, 0.0f), rot_b);
	float3 cap_p_a = pos_b - up_b * half_height_b;
	float3 cap_p_b = pos_b + up_b * half_height_b;

	float3 closest_on_cap = closest_point_on_segment(pos_a, cap_p_a, cap_p_b);

	float3 delta = closest_on_cap - pos_a;
	float dist_sq = dot(delta, delta);

	if (dist_sq > radius_sum * radius_sum) return false;

	float distance = sqrt(dist_sq);
	if (distance < 0.0001f) {
		depth = radius_sum;
		normal = float3(0.0f, 1.0f, 0.0f);
	} else {
		depth = radius_sum - distance;
		normal = delta * (-1.0f / distance);
	}

	point_a = pos_a + normal * -radius_a;
	point_b = closest_on_cap + normal * radius_b;

	return true;
}

bool collision_test_capsule_capsule(float3 pos_a, float4 rot_a, float half_height_a, float radius_a,
									float3 pos_b, float4 rot_b, float half_height_b, float radius_b,
									out float depth, out float3 normal, out float3 point_a,
									out float3 point_b) {
	depth = 0.0f;
	normal = float3(0.0f, 0.0f, 0.0f);
	point_a = float3(0.0f, 0.0f, 0.0f);
	point_b = float3(0.0f, 0.0f, 0.0f);

	float3 up_a = rotate_vector(float3(0.0f, 1.0f, 0.0f), rot_a);
	float3 a_p_a = pos_a - up_a * half_height_a;
	float3 a_p_b = pos_a + up_a * half_height_a;

	float3 up_b = rotate_vector(float3(0.0f, 1.0f, 0.0f), rot_b);
	float3 b_p_a = pos_b - up_b * half_height_b;
	float3 b_p_b = pos_b + up_b * half_height_b;

	float3 closest_a, closest_b;
	closest_points_between_segments(a_p_a, a_p_b, b_p_a, b_p_b, closest_a, closest_b);

	float radius_sum = radius_a + radius_b;
	float3 delta = closest_b - closest_a;
	float dist_sq = dot(delta, delta);

	if (dist_sq > radius_sum * radius_sum) return false;

	float distance = sqrt(dist_sq);
	if (distance < 0.0001f) {
		depth = radius_sum;
		normal = float3(0.0f, 1.0f, 0.0f);
	} else {
		depth = radius_sum - distance;
		normal = delta * (-1.0f / distance);
	}

	point_a = closest_a + normal * -radius_a;
	point_b = closest_b + normal * radius_b;

	return true;
}

bool collision_test_sphere_obb(float3 pos_a, float radius_a, float3 pos_b, float4 rot_b,
							   float3 extents_b, out float depth, out float3 normal,
							   out float3 point_a, out float3 point_b) {
	depth = 0.0f;
	normal = float3(0.0f, 0.0f, 0.0f);
	point_a = float3(0.0f, 0.0f, 0.0f);
	point_b = float3(0.0f, 0.0f, 0.0f);

	float3 local_center = world_to_local(pos_a, pos_b, rot_b);
	float3 clamped = clamp(local_center, -extents_b, extents_b);
	float3 delta = local_center - clamped;
	float dist_sq = dot(delta, delta);

	float3 local_normal;

	if (dist_sq < 0.00001f) {
		float3 dist_to_face = extents_b - abs(local_center);

		if (dist_to_face.x <= dist_to_face.y && dist_to_face.x <= dist_to_face.z) {
			clamped.x = local_center.x > 0.0f ? extents_b.x : -extents_b.x;
			local_normal = float3(local_center.x > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
			depth = radius_a + dist_to_face.x;
		} else if (dist_to_face.y <= dist_to_face.x && dist_to_face.y <= dist_to_face.z) {
			clamped.y = local_center.y > 0.0f ? extents_b.y : -extents_b.y;
			local_normal = float3(0.0f, local_center.y > 0.0f ? 1.0f : -1.0f, 0.0f);
			depth = radius_a + dist_to_face.y;
		} else {
			clamped.z = local_center.z > 0.0f ? extents_b.z : -extents_b.z;
			local_normal = float3(0.0f, 0.0f, local_center.z > 0.0f ? 1.0f : -1.0f);
			depth = radius_a + dist_to_face.z;
		}
	} else {
		if (dist_sq > radius_a * radius_a) return false;

		float dist = sqrt(dist_sq);
		depth = radius_a - dist;
		local_normal = delta / dist;
	}

	normal = rotate_vector(local_normal, rot_b);
	point_b = local_to_world(clamped, pos_b, rot_b);
	point_a = pos_a + normal * -radius_a;

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

	if (abs_x <= abs_y && abs_x <= abs_z) {
		edge_start.x = -ext.x; edge_end.x = ext.x;
	} else if (abs_y <= abs_x && abs_y <= abs_z) {
		edge_start.y = -ext.y; edge_end.y = ext.y;
	} else {
		edge_start.z = -ext.z; edge_end.z = ext.z;
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
		overlap = d1; push_n = n;
	} else {
		overlap = d2; push_n = -n;
	}

	if (overlap < min_overlap) {
		if (best_type == 0 && type == 1 && overlap > min_overlap * 0.999f) return true;
		min_overlap = overlap; best_axis = push_n; best_type = type;
	}
	return true;
}

bool collision_test_capsule_obb(float3 pos_a, float4 rot_a, float half_height_a, float radius_a,
								float3 pos_b, float4 rot_b, float3 extents_b, out float depth,
								out float3 normal, out float3 point_a, out float3 point_b) {
	depth = 0.0f;
	normal = float3(0.0f, 0.0f, 0.0f);
	point_a = float3(0.0f, 0.0f, 0.0f);
	point_b = float3(0.0f, 0.0f, 0.0f);

	float3 up_a = rotate_vector(float3(0.0f, 1.0f, 0.0f), rot_a);
	float3 world_a = pos_a - up_a * half_height_a;
	float3 world_b = pos_a + up_a * half_height_a;

	float3 A = world_to_local(world_a, pos_b, rot_b);
	float3 B = world_to_local(world_b, pos_b, rot_b);
	float3 D = B - A;

	float3 exp_ext = extents_b + radius_a;

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

	float t0 = max(0.0f, t_min_ray);
	float t1 = min(1.0f, t_max_ray);

	const float inv_phi = 0.6180339887f;
	const float inv_phi_2 = 0.3819660113f;

	float t_a = t0 + inv_phi_2 * (t1 - t0);
	float t_b = t0 + inv_phi * (t1 - t0);

	float3 pos_sa = A + D * t_a;
	float3 proj_a = clamp(pos_sa, -extents_b, extents_b);
	float3 d_a = pos_sa - proj_a;
	float dist_sq_a = dot(d_a, d_a);

	float3 pos_sb = A + D * t_b;
	float3 proj_b = clamp(pos_sb, -extents_b, extents_b);
	float3 d_b = pos_sb - proj_b;
	float dist_sq_b = dot(d_b, d_b);

	for (int iter = 0; iter < 32; iter++) {
		if (dist_sq_a < dist_sq_b) {
			t1 = t_b; t_b = t_a; dist_sq_b = dist_sq_a;
			t_a = t0 + inv_phi_2 * (t1 - t0);
			pos_sa = A + D * t_a; proj_a = clamp(pos_sa, -extents_b, extents_b);
			float3 temp_d = pos_sa - proj_a; dist_sq_a = dot(temp_d, temp_d);
		} else {
			t0 = t_a; t_a = t_b; dist_sq_a = dist_sq_b;
			t_b = t0 + inv_phi * (t1 - t0);
			pos_sb = A + D * t_b; proj_b = clamp(pos_sb, -extents_b, extents_b);
			float3 temp_d = pos_sb - proj_b; dist_sq_b = dot(temp_d, temp_d);
		}
	}

	float best_t = (t0 + t1) * 0.5f;
	float3 p_seg = A + D * best_t;
	float3 q_box = clamp(p_seg, -extents_b, extents_b);

	float3 delta = p_seg - q_box;
	float dist_sq = dot(delta, delta);

	if (dist_sq > radius_a * radius_a) return false;

	if (dist_sq > 0.00001f) {
		float dist = sqrt(dist_sq);
		depth = radius_a - dist;
		float3 local_normal = delta * (1.0f / dist);

		normal = rotate_vector(local_normal, rot_b);
		point_b = local_to_world(q_box, pos_b, rot_b);
		point_a = point_b - normal * depth;

		return true;
	}

	float min_overlap = 3.402823466e+38f;
	float3 best_axis = float3(0.0f, 0.0f, 0.0f);
	int best_type = -1;

	if (!test_capsule_box_axis(float3(1.0,0.0,0.0), extents_b, A, B, radius_a, min_overlap, best_axis, 0, best_type)) return false;
	if (!test_capsule_box_axis(float3(0.0,1.0,0.0), extents_b, A, B, radius_a, min_overlap, best_axis, 0, best_type)) return false;
	if (!test_capsule_box_axis(float3(0.0,0.0,1.0), extents_b, A, B, radius_a, min_overlap, best_axis, 0, best_type)) return false;

	if (!test_capsule_box_axis(float3(0.0f, -D.z, D.y), extents_b, A, B, radius_a, min_overlap, best_axis, 1, best_type)) return false;
	if (!test_capsule_box_axis(float3(D.z, 0.0f, -D.x), extents_b, A, B, radius_a, min_overlap, best_axis, 1, best_type)) return false;
	if (!test_capsule_box_axis(float3(-D.y, D.x, 0.0f), extents_b, A, B, radius_a, min_overlap, best_axis, 1, best_type)) return false;

	depth = min_overlap;
	normal = rotate_vector(best_axis, rot_b);

	float dot_a = dot(world_a, normal);
	float dot_b = dot(world_b, normal);
	float3 deepest_core = (dot_a < dot_b) ? world_a : world_b;

	if (best_type == 0) {
		point_a = deepest_core + normal * -radius_a;
	} else {
		float3 box_p1, box_p2;
		get_box_support_edge(extents_b, pos_b, rot_b, normal, box_p1, box_p2);

		float3 core_a;
		closest_points_between_segments(world_a, world_b, box_p1, box_p2, core_a, point_b);
		point_a = core_a + normal * -radius_a;
	}

	point_b = point_a + normal * depth;

	return true;
}

bool test_edge_axis(float3 axis, float r_a, float r_b, float abs_t,
					inout float min_overlap, inout float3 best_axis, inout int best_type) {
	float len_sq = dot(axis, axis);
	if (len_sq > 0.00001f) {
		float len = sqrt(len_sq);
		float overlap = (r_a + r_b - abs_t) / len;

		if (overlap < 0.0f) return false;

		if (overlap < min_overlap) {
			if (best_type < 2 && overlap > min_overlap * 0.999f) return true;
			min_overlap = overlap; best_axis = axis * (1.0f / len); best_type = 2;
		}
	}
	return true;
}

bool collision_test_obb_obb(float3 pos_a, float4 rot_a, float3 extents_a,
							float3 pos_b, float4 rot_b, float3 extents_b,
							out float depth, out float3 normal, out float3 point_a,
							out float3 point_b) {
	depth = 0.0f;
	normal = float3(0.0f, 0.0f, 0.0f);
	point_a = float3(0.0f, 0.0f, 0.0f);
	point_b = float3(0.0f, 0.0f, 0.0f);

	float3 T = world_to_local(pos_b, pos_a, rot_a);
	float4 rel_q = quat_mul(quat_inverse(rot_a), rot_b);

	float xx = rel_q.x * rel_q.x, yy = rel_q.y * rel_q.y, zz = rel_q.z * rel_q.z;
	float xy = rel_q.x * rel_q.y, xz = rel_q.x * rel_q.z, yz = rel_q.y * rel_q.z;
	float wx = rel_q.w * rel_q.x, wy = rel_q.w * rel_q.y, wz = rel_q.w * rel_q.z;

	float3 r_x = float3(1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz),       2.0f * (xz - wy));
	float3 r_y = float3(2.0f * (xy - wz),       1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx));
	float3 r_z = float3(2.0f * (xz + wy),       2.0f * (yz - wx),       1.0f - 2.0f * (xx + yy));

	float3 abs_rx = abs(r_x) + 0.000001f;
	float3 abs_ry = abs(r_y) + 0.000001f;
	float3 abs_rz = abs(r_z) + 0.000001f;

	float min_overlap = 3.402823466e+38f;
	float3 best_axis = float3(0.0f, 0.0f, 0.0f);
	int best_type = -1;
	float ra, rb, overlap;

	// 3 Face Axes of Box A
	ra = extents_a.x; rb = dot(extents_b, float3(abs_rx.x, abs_ry.x, abs_rz.x));
	overlap = ra + rb - abs(T.x); if (overlap < 0.0f) return false;
	min_overlap = overlap; best_axis = float3(1.0f, 0.0f, 0.0f); best_type = 0;

	ra = extents_a.y; rb = dot(extents_b, float3(abs_rx.y, abs_ry.y, abs_rz.y));
	overlap = ra + rb - abs(T.y); if (overlap < 0.0f) return false;
	if (overlap < min_overlap) { min_overlap = overlap; best_axis = float3(0.0f, 1.0f, 0.0f); best_type = 0; }

	ra = extents_a.z; rb = dot(extents_b, float3(abs_rx.z, abs_ry.z, abs_rz.z));
	overlap = ra + rb - abs(T.z); if (overlap < 0.0f) return false;
	if (overlap < min_overlap) { min_overlap = overlap; best_axis = float3(0.0f, 0.0f, 1.0f); best_type = 0; }

	// 3 Face Axes of Box B
	ra = dot(extents_a, abs_rx); rb = extents_b.x;
	overlap = ra + rb - abs(dot(T, r_x)); if (overlap < 0.0f) return false;
	if (overlap < min_overlap) { min_overlap = overlap; best_axis = r_x; best_type = 1; }

	ra = dot(extents_a, abs_ry); rb = extents_b.y;
	overlap = ra + rb - abs(dot(T, r_y)); if (overlap < 0.0f) return false;
	if (overlap < min_overlap) { min_overlap = overlap; best_axis = r_y; best_type = 1; }

	ra = dot(extents_a, abs_rz); rb = extents_b.z;
	overlap = ra + rb - abs(dot(T, r_z)); if (overlap < 0.0f) return false;
	if (overlap < min_overlap) { min_overlap = overlap; best_axis = r_z; best_type = 1; }

	// 9 Edge-Edge Axes
	if (!test_edge_axis(float3(0.0f, -r_x.z, r_x.y), extents_a.y * abs_rx.z + extents_a.z * abs_rx.y, extents_b.y * abs_rz.x + extents_b.z * abs_ry.x, abs(T.y * r_x.z - T.z * r_x.y), min_overlap, best_axis, best_type)) return false;
	if (!test_edge_axis(float3(0.0f, -r_y.z, r_y.y), extents_a.y * abs_ry.z + extents_a.z * abs_ry.y, extents_b.x * abs_rz.x + extents_b.z * abs_rx.x, abs(T.y * r_y.z - T.z * r_y.y), min_overlap, best_axis, best_type)) return false;
	if (!test_edge_axis(float3(0.0f, -r_z.z, r_z.y), extents_a.y * abs_rz.z + extents_a.z * abs_rz.y, extents_b.x * abs_ry.x + extents_b.y * abs_rx.x, abs(T.y * r_z.z - T.z * r_z.y), min_overlap, best_axis, best_type)) return false;
	if (!test_edge_axis(float3(r_x.z, 0.0f, -r_x.x), extents_a.x * abs_rx.z + extents_a.z * abs_rx.x, extents_b.y * abs_rz.y + extents_b.z * abs_ry.y, abs(T.z * r_x.x - T.x * r_x.z), min_overlap, best_axis, best_type)) return false;
	if (!test_edge_axis(float3(r_y.z, 0.0f, -r_y.x), extents_a.x * abs_ry.z + extents_a.z * abs_ry.x, extents_b.x * abs_rz.y + extents_b.z * abs_rx.y, abs(T.z * r_y.x - T.x * r_y.z), min_overlap, best_axis, best_type)) return false;
	if (!test_edge_axis(float3(r_z.z, 0.0f, -r_z.x), extents_a.x * abs_rz.z + extents_a.z * abs_rz.x, extents_b.x * abs_ry.y + extents_b.y * abs_rx.y, abs(T.z * r_z.x - T.x * r_z.z), min_overlap, best_axis, best_type)) return false;
	if (!test_edge_axis(float3(-r_x.y, r_x.x, 0.0f), extents_a.x * abs_rx.y + extents_a.y * abs_rx.x, extents_b.y * abs_rz.z + extents_b.z * abs_ry.z, abs(T.x * r_x.y - T.y * r_x.x), min_overlap, best_axis, best_type)) return false;
	if (!test_edge_axis(float3(-r_y.y, r_y.x, 0.0f), extents_a.x * abs_ry.y + extents_a.y * abs_ry.x, extents_b.x * abs_rz.z + extents_b.z * abs_rx.z, abs(T.x * r_y.y - T.y * r_y.x), min_overlap, best_axis, best_type)) return false;
	if (!test_edge_axis(float3(-r_z.y, r_z.x, 0.0f), extents_a.x * abs_rz.y + extents_a.y * abs_rz.x, extents_b.x * abs_ry.z + extents_b.y * abs_rx.z, abs(T.x * r_z.y - T.y * r_z.x), min_overlap, best_axis, best_type)) return false;

	if (min_overlap <= 0.0f) return false;

	depth = min_overlap;

	if (dot(best_axis, T) > 0.0f) best_axis = -best_axis;

	normal = rotate_vector(best_axis, rot_a);

	if (best_type == 0) {
		point_b = get_box_support_point(extents_b, pos_b, rot_b, normal);
		point_a = point_b - normal * depth;
	} else if (best_type == 1) {
		point_a = get_box_support_point(extents_a, pos_a, rot_a, -normal);
		point_b = point_a + normal * depth;
	} else {
		float3 a_p1, a_p2, b_p1, b_p2;
		get_box_support_edge(extents_a, pos_a, rot_a, -normal, a_p1, a_p2);
		get_box_support_edge(extents_b, pos_b, rot_b, normal, b_p1, b_p2);
		closest_points_between_segments(a_p1, a_p2, b_p1, b_p2, point_a, point_b);
	}

	return true;
}

bool evaluate_narrow_phase(dx_entity e_a, dx_shape s_a, dx_entity e_b, dx_shape s_b,
						   out float depth, out float3 normal, out float3 point_a,
						   out float3 point_b) {
	depth = 0.0f;
	normal = float3(0.0f, 0.0f, 0.0f);
	point_a = float3(0.0f, 0.0f, 0.0f);
	point_b = float3(0.0f, 0.0f, 0.0f);

	bool swapped = (e_a.shape_info >> 30) > (e_b.shape_info >> 30);
	
	float3 p_a = swapped ? e_b.position : e_a.position;
	float4 r_a = swapped ? e_b.rotation : e_a.rotation;
	float4 d_a = swapped ? s_b.data : s_a.data;
	uint type_a = swapped ? (e_b.shape_info >> 30) : (e_a.shape_info >> 30);

	float3 p_b = swapped ? e_a.position : e_b.position;
	float4 r_b = swapped ? e_a.rotation : e_b.rotation;
	float4 d_b = swapped ? s_a.data : s_b.data;
	uint type_b = swapped ? (e_a.shape_info >> 30) : (e_b.shape_info >> 30);

	bool hit = false;
	if (type_a == 0 && type_b == 0) {
		hit = collision_test_sphere_sphere(p_a, d_a.x, p_b, d_b.x, depth, normal, point_a, point_b);
	} else if (type_a == 0 && type_b == 1) {
		hit = collision_test_sphere_capsule(p_a, d_a.x, p_b, r_b, d_b.x, d_b.y, depth, normal, point_a, point_b);
	} else if (type_a == 1 && type_b == 1) {
		hit = collision_test_capsule_capsule(p_a, r_a, d_a.x, d_a.y, p_b, r_b, d_b.x, d_b.y, depth, normal, point_a, point_b);
	} else if (type_a == 0 && type_b == 2) {
		hit = collision_test_sphere_obb(p_a, d_a.x, p_b, r_b, d_b.xyz, depth, normal, point_a, point_b);
	} else if (type_a == 1 && type_b == 2) {
		hit = collision_test_capsule_obb(p_a, r_a, d_a.x, d_a.y, p_b, r_b, d_b.xyz, depth, normal, point_a, point_b);
	} else { // Implicitly (type_a == 2 && type_b == 2)
		hit = collision_test_obb_obb(p_a, r_a, d_a.xyz, p_b, r_b, d_b.xyz, depth, normal, point_a, point_b);
	}

	if (swapped && hit) {
		normal = -normal;
		float3 temp_pt = point_a;
		point_a = point_b;
		point_b = temp_pt;
	}

	return hit;
}
