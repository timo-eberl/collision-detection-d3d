struct dx_shape {
	float3 p_a;
	float radius;
	float3 p_b;
	uint type;
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
	uint rigid_count;
	uint static_count;
	uint max_collisions;
};

StructuredBuffer<dx_shape> rigids : register(t0);
StructuredBuffer<dx_shape> statics : register(t1);

RWStructuredBuffer<dx_collision> collisions : register(u0);
RWStructuredBuffer<uint> col_count : register(u1);

struct aabb {
	float3 min_p;
	float3 max_p;
};

aabb compute_aabb(dx_shape s) {
	aabb box;
	if (s.type == 0) {
		box.min_p = s.p_a - s.radius;
		box.max_p = s.p_a + s.radius;
	} else {
		box.min_p = min(s.p_a, s.p_b) - s.radius;
		box.max_p = max(s.p_a, s.p_b) + s.radius;
	}
	return box;
}

bool aabb_overlap(aabb a, aabb b) {
	return a.max_p.x >= b.min_p.x && a.min_p.x <= b.max_p.x &&
	       a.max_p.y >= b.min_p.y && a.min_p.y <= b.max_p.y &&
	       a.max_p.z >= b.min_p.z && a.min_p.z <= b.max_p.z;
}

float3 closest_point_on_segment(float3 p, float3 a, float3 b) {
	float3 ab = b - a;
	float len_sq = dot(ab, ab);

	if (len_sq <= 0.00001f) return a;

	float t = saturate(dot(p - a, ab) / len_sq);
	return a + ab * t;
}

// Calculates the shortest distance between two 3D line segments by solving a parameterized
// linear system for variables s and t. The results are clamped to [0, 1] to ensure the closest
// points fall strictly on the bounded segments.
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

bool collision_test_sphere_sphere(dx_shape a, dx_shape b, out dx_collision result) {
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

bool collision_test_sphere_capsule(dx_shape a, dx_shape b, out dx_collision result) {
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

bool collision_test_capsule_capsule(dx_shape a, dx_shape b, out dx_collision result) {
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

bool evaluate_narrow_phase(dx_shape a, dx_shape b, out dx_collision result) {
	bool swapped = false;
	
	if (a.type == 1 && b.type == 0) {
		dx_shape temp_shape = a;
		a = b;
		b = temp_shape;
		swapped = true;
	}

	bool hit = false;
	if (a.type == 0 && b.type == 0) {
		hit = collision_test_sphere_sphere(a, b, result);
	} else if (a.type == 0 && b.type == 1) {
		hit = collision_test_sphere_capsule(a, b, result);
	} else {
		hit = collision_test_capsule_capsule(a, b, result);
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
void main(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= rigid_count) return;

	dx_shape shape_i = rigids[i];
	aabb box_i = compute_aabb(shape_i);

	for (uint j = i + 1; j < rigid_count; ++j) {
		dx_shape shape_j = rigids[j];
		if (aabb_overlap(box_i, compute_aabb(shape_j))) {
			dx_collision c;
			if (evaluate_narrow_phase(shape_i, shape_j, c)) {
				uint idx;
				InterlockedAdd(col_count[0], 1, idx);
				if (idx < max_collisions) {
					c.a_index = i;
					c.b_index = j;
					c.b_type = 1;
					collisions[idx] = c;
				}
			}
		}
	}

	for (uint k = 0; k < static_count; ++k) {
		dx_shape shape_k = statics[k];
		if (aabb_overlap(box_i, compute_aabb(shape_k))) {
			dx_collision c;
			if (evaluate_narrow_phase(shape_i, shape_k, c)) {
				uint idx;
				InterlockedAdd(col_count[0], 1, idx);
				if (idx < max_collisions) {
					c.a_index = i;
					c.b_index = k;
					c.b_type = 0;
					collisions[idx] = c;
				}
			}
		}
	}
}
