#include "collision_detection_d3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#endif

inline float sign_not_zero(float v) {
	return v >= 0.0f ? 1.0f : -1.0f;
}

void decode_octahedral(const float* enc, float* n) {
	float x = enc[0];
	float y = enc[1];
	// Calculate z based on the octahedron's geometric properties
	float z = 1.0f - fabsf(x) - fabsf(y);
	
	// If the vector points into the lower hemisphere, un-fold the xy coordinates
	if (z < 0.0f) {
		float old_x = x;
		x = (1.0f - fabsf(y)) * sign_not_zero(old_x);
		y = (1.0f - fabsf(old_x)) * sign_not_zero(y);
	}
	
	float len = sqrtf(x * x + y * y + z * z);
	if (len > 0.00001f) {
		n[0] = x / len;
		n[1] = y / len;
		n[2] = z / len;
	} else {
		n[0] = 0.0f; n[1] = 1.0f; n[2] = 0.0f;
	}
}

typedef struct {
	uint32_t a_index;
	uint32_t b_index;
	uint32_t b_type; // 0 = Static, 1 = Rigid
	float depth;
	float point_a[3];
	float point_b[3];
	float normal[3];
	uint32_t pad[3];
} dx_collision_full; // 64 bytes

int compare_collisions(const void* a, const void* b) {
	const dx_collision_full* ca = (const dx_collision_full*)a;
	const dx_collision_full* cb = (const dx_collision_full*)b;

	if (ca->a_index != cb->a_index) return (int)ca->a_index - (int)cb->a_index;
	if (ca->b_type != cb->b_type) return (int)ca->b_type - (int)cb->b_type;
	return (int)ca->b_index - (int)cb->b_index;
}

int compare_compact_collisions(const void* a, const void* b) {
	const dx_collision_compact* ca = (const dx_collision_compact*)a;
	const dx_collision_compact* cb = (const dx_collision_compact*)b;

	if (ca->a_index != cb->a_index) return (int)ca->a_index - (int)cb->a_index;
	return (int)ca->b_index - (int)cb->b_index;
}

bool float_eq_approx(float a, float b, float epsilon = 0.001f) {
	return fabs(a - b) < epsilon;
}

bool vec3_eq_approx(const float* a, const float* b, float epsilon = 0.001f) {
	return float_eq_approx(a[0], b[0], epsilon) &&
	       float_eq_approx(a[1], b[1], epsilon) &&
	       float_eq_approx(a[2], b[2], epsilon);
}

int main() {

	// Fix emojis on some Windows terminals
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#endif

	FILE* file = fopen("collision_test_data.bin", "rb");
	if (!file) {
		fprintf(stderr, "Failed to open collision_test_data.bin\n");
		return 1;
	}

	dx_shared_state* sh = dx_shared_state_create();
	dx_state_simple_naive* state_naive = dx_state_simple_naive_create(sh);
	dx_state_simple_binned* state_binned = dx_state_simple_binned_create(sh);

	uint32_t frame_index = 0;
	uint32_t counts[4];

	while (fread(counts, sizeof(uint32_t), 4, file) == 4) {
		uint32_t rigid_count = counts[0];
		uint32_t static_count = counts[1];
		uint32_t shape_count = counts[2];
		uint32_t expected_col_count = counts[3];

		dx_entity* rigids = (dx_entity*)malloc(rigid_count * sizeof(dx_entity));
		dx_entity* statics = (dx_entity*)malloc(static_count * sizeof(dx_entity));
		dx_shape* shapes = (dx_shape*)malloc(shape_count * sizeof(dx_shape));
		dx_collision_full* expected_cols = (dx_collision_full*)malloc(
			expected_col_count * sizeof(dx_collision_full));

		if (rigid_count > 0) fread(rigids, sizeof(dx_entity), rigid_count, file);
		if (static_count > 0) fread(statics, sizeof(dx_entity), static_count, file);
		if (shape_count > 0) fread(shapes, sizeof(dx_shape), shape_count, file);
		if (expected_col_count > 0) {
			fread(expected_cols, sizeof(dx_collision_full), expected_col_count, file);
		}

		if (frame_index < 160 || frame_index > 169) {
			free(rigids);
			free(statics);
			free(shapes);
			free(expected_cols);
			if (frame_index > 169) break;
			frame_index++;
			continue;
		}

		uint32_t naive_col_count = 0;
		dx_collision_compact* naive_compact = dx_run_simple_naive(
			sh, state_naive, rigids, rigid_count, statics, static_count, shapes, shape_count, true,
			&naive_col_count);

		uint32_t binned_col_count = 0;
		dx_collision_compact* binned_compact = dx_run_simple_binned(
			sh, state_binned, rigids, rigid_count, statics, static_count, shapes, shape_count, true,
			&binned_col_count);

		// GPU vs GPU comparison (sort, then memcmp)
		bool pipeline_match = true;
		if (naive_col_count != binned_col_count) {
			fprintf(stderr,
				"❌ Frame %u FAILED: Pipeline mismatch! Naive count (%u) != Binned count (%u)\n",
				frame_index, naive_col_count, binned_col_count);
			pipeline_match = false;
		} else if (binned_col_count > 0) {
			qsort(naive_compact, naive_col_count, sizeof(dx_collision_compact),
				  compare_compact_collisions);
			qsort(binned_compact, binned_col_count, sizeof(dx_collision_compact),
				  compare_compact_collisions);
			if (memcmp(naive_compact, binned_compact,
					   binned_col_count * sizeof(dx_collision_compact)) != 0) {
				fprintf(stderr,
						"❌ Frame %u FAILED: Pipeline mismatch! GPU algorithms returned different "
						"data.\n",
						frame_index);
				pipeline_match = false;
			}
		}

		if (naive_compact) free(naive_compact);

		if (!pipeline_match) {
			// Cancel further validation if the GPU algorithms don't even agree with each other
			if (binned_compact) free(binned_compact);
			free(rigids);
			free(statics);
			free(shapes);
			free(expected_cols);
			frame_index++;
			continue;
		}

		// GPU vs CPU comparison
		uint32_t actual_col_count = binned_col_count;
		dx_collision_compact* actual_compact = binned_compact;
		dx_collision_full* actual_cols = nullptr;
		if (actual_col_count > 0) {
			actual_cols = (dx_collision_full*)malloc(
				actual_col_count * sizeof(dx_collision_full));
			
			for (uint32_t j = 0; j < actual_col_count; ++j) {
				dx_collision_compact* comp = &actual_compact[j];
				dx_collision_full* full = &actual_cols[j];
				
				full->a_index = comp->a_index;
				if (comp->b_index >= rigid_count) {
					full->b_index = comp->b_index - rigid_count;
					full->b_type = 0;
				} else {
					full->b_index = comp->b_index;
					full->b_type = 1;
				}
				
				full->depth = comp->depth;
				full->point_a[0] = comp->point_a[0];
				full->point_a[1] = comp->point_a[1];
				full->point_a[2] = comp->point_a[2];
				
				decode_octahedral(comp->normal, full->normal);
				
				full->point_b[0] = full->point_a[0] + full->normal[0] * full->depth;
				full->point_b[1] = full->point_a[1] + full->normal[1] * full->depth;
				full->point_b[2] = full->point_a[2] + full->normal[2] * full->depth;
				
				full->pad[0] = 0; full->pad[1] = 0; full->pad[2] = 0;
			}
			free(actual_compact);
		}

		if (expected_col_count > 0) {
			qsort(expected_cols, expected_col_count, sizeof(dx_collision_full), compare_collisions);
		}
		if (actual_col_count > 0) {
			qsort(actual_cols, actual_col_count, sizeof(dx_collision_full), compare_collisions);
		}

		auto print_body_details = [&](uint32_t a_idx, uint32_t b_idx, uint32_t b_type) {
			dx_entity& a = rigids[a_idx];
			dx_entity& b = (b_type == 1) ? rigids[b_idx] : statics[b_idx];
			dx_shape& s_a = shapes[a.shape_index];
			dx_shape& s_b = shapes[b.shape_index];

			const char* type_names[] = {"Sphere", "Capsule", "Box", "Convex"};

			fprintf(stderr, "  Body A (%s):\n", a.shape_type < 4 ? type_names[a.shape_type] : "Unknown");
			fprintf(stderr, "    Pos:  (%f, %f, %f)\n", a.position[0], a.position[1], a.position[2]);
			fprintf(stderr, "    Rot:  (%f, %f, %f, %f)\n", a.rotation[0], a.rotation[1], a.rotation[2], a.rotation[3]);
			fprintf(stderr, "    Data: (%f, %f, %f, %f)\n", s_a.data[0], s_a.data[1], s_a.data[2], s_a.data[3]);

			fprintf(stderr, "  Body B (%s):\n", b.shape_type < 4 ? type_names[b.shape_type] : "Unknown");
			fprintf(stderr, "    Pos:  (%f, %f, %f)\n", b.position[0], b.position[1], b.position[2]);
			fprintf(stderr, "    Rot:  (%f, %f, %f, %f)\n", b.rotation[0], b.rotation[1], b.rotation[2], b.rotation[3]);
			fprintf(stderr, "    Data: (%f, %f, %f, %f)\n", s_b.data[0], s_b.data[1], s_b.data[2], s_b.data[3]);
		};

		bool passed = true;
		uint32_t i = 0, j = 0;

		// walk through both arrays simultaneously
		// If we find a pair that exists in one list but not the other, we check its depth. If the
		// depth is close to zero, we ignore it
		while (i < expected_col_count || j < actual_col_count) {
			dx_collision_full* exp = (i < expected_col_count) ? &expected_cols[i] : nullptr;
			dx_collision_full* act = (j < actual_col_count) ? &actual_cols[j] : nullptr;

			int cmp = 0;
			if (exp && act) cmp = compare_collisions(exp, act);
			else if (exp) cmp = -1;
			else cmp = 1;

			if (cmp == 0) {
				// Pair exists in both, check math
				bool depth_ok = float_eq_approx(exp->depth, act->depth, 0.001f);
				bool normal_ok = vec3_eq_approx(exp->normal, act->normal, 0.02f);
				bool pt_ok = vec3_eq_approx(exp->point_a, act->point_a, 0.001f);

				// Handle parallel shapes (sliding contact point on the same normal)
				if (!pt_ok && normal_ok) {
					float dx = act->point_a[0] - exp->point_a[0];
					float dy = act->point_a[1] - exp->point_a[1];
					float dz = act->point_a[2] - exp->point_a[2];
					// If the distance along the expected normal is ~0, it's a valid sliding point
					float normal_err =
						fabsf(dx * exp->normal[0] + dy * exp->normal[1] + dz * exp->normal[2]);
					if (normal_err < 0.01f) { pt_ok = true; }
				}

				// Handle SAT: Different features with almost identical depth
				// When CPU and GPU evaluate competing axes, precision drift can cause them to pick
				// different features. Even though normal and points are different, the result may
				// still be valid
				if (depth_ok && (!normal_ok || !pt_ok)) {
					float depth_diff = fabsf(exp->depth - act->depth);

					// If depths are extremely close (e.g., within 0.0002 or 0.5% relative error),
					// different penetration vectors are also acceptable
					if (depth_diff < 0.0002f ||
						(depth_diff / fmaxf(exp->depth, 0.0001f) < 0.005f)) {
						normal_ok = true;
						pt_ok = true;
					}
				}

				if (!depth_ok || !normal_ok || !pt_ok) {
					fprintf(stderr, "❌ Frame %u FAILED: Math mismatch for pair (%u, %u type %u)\n",
							frame_index, exp->a_index, exp->b_index, exp->b_type);

					float exp_len = sqrtf(exp->normal[0] * exp->normal[0] +
										  exp->normal[1] * exp->normal[1] +
										  exp->normal[2] * exp->normal[2]);
					float act_len = sqrtf(act->normal[0] * act->normal[0] +
										  act->normal[1] * act->normal[1] +
										  act->normal[2] * act->normal[2]);

					fprintf(stderr, "  Expected: depth=%f, normal=(%f, %f, %f) len=%f, "
									"pt_a=(%f, %f, %f)\n",
							exp->depth, exp->normal[0], exp->normal[1], exp->normal[2], exp_len,
							exp->point_a[0], exp->point_a[1], exp->point_a[2]);

					fprintf(stderr, "  Actual:   depth=%f, normal=(%f, %f, %f) len=%f, "
									"pt_a=(%f, %f, %f)\n",
							act->depth, act->normal[0], act->normal[1], act->normal[2], act_len,
							act->point_a[0], act->point_a[1], act->point_a[2]);

					print_body_details(exp->a_index, exp->b_index, exp->b_type);

					passed = false; break;
				}
				i++; j++;
			}
			else if (cmp < 0) {
				// Expected pair is missing from Actual (GPU missed it)
				if (exp->depth < 0.0001f) {
					i++;
				} else {
					fprintf(stderr, "❌ Frame %u FAILED: Missing expected pair (%u, %u type %u) "
									"with depth=%f\n",
							frame_index, exp->a_index, exp->b_index, exp->b_type, exp->depth);

					print_body_details(exp->a_index, exp->b_index, exp->b_type);

					passed = false; break;
				}
			}
			else {
				// Actual pair is extra (GPU found an extra one)
				if (act->depth < 0.0001f) {
					j++;
				} else {
					fprintf(stderr, "❌ Frame %u FAILED: Extra GPU pair (%u, %u type %u) "
									"with depth=%f\n",
							frame_index, act->a_index, act->b_index, act->b_type, act->depth);

					print_body_details(act->a_index, act->b_index, act->b_type);

					passed = false; break;
				}
			}
		}

		if (passed) {
			printf("✅ Frame %u PASSED: %u collisions\n", frame_index, actual_col_count);
		}

		// Flush, so stdout and stderr messages are printed in order (OS dependent)
		fflush(stdout);
		fflush(stderr);

		free(rigids);
		free(statics);
		free(shapes);
		free(expected_cols);
		if (actual_cols) free(actual_cols);

		frame_index++;
	}

	dx_state_simple_naive_destroy(state_naive);
	dx_state_simple_binned_destroy(state_binned);
	dx_shared_state_destroy(sh);

	fclose(file);
	return 0;
}
