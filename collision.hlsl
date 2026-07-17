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

[numthreads(256, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
	uint i = DTid.x;
	if (i >= rigid_count) return;

	aabb box_i = compute_aabb(rigids[i]);

	for (uint j = i + 1; j < rigid_count; ++j) {
		if (aabb_overlap(box_i, compute_aabb(rigids[j]))) {
			uint idx;
			InterlockedAdd(col_count[0], 1, idx);
			if (idx < max_collisions) {
				dx_collision c = (dx_collision)0;
				c.a_index = i;
				c.b_index = j;
				c.b_type = 1;
				collisions[idx] = c;
			}
		}
	}

	for (uint k = 0; k < static_count; ++k) {
		if (aabb_overlap(box_i, compute_aabb(statics[k]))) {
			uint idx;
			InterlockedAdd(col_count[0], 1, idx);
			if (idx < max_collisions) {
				dx_collision c = (dx_collision)0;
				c.a_index = i;
				c.b_index = k;
				c.b_type = 0;
				collisions[idx] = c;
			}
		}
	}
}
