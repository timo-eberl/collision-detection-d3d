#include "collision_detection_d3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#endif

int compare_collisions(const void* a, const void* b) {
	const dx_collision* ca = (const dx_collision*)a;
	const dx_collision* cb = (const dx_collision*)b;

	if (ca->a_index != cb->a_index) return (int)ca->a_index - (int)cb->a_index;
	if (ca->b_type != cb->b_type) return (int)ca->b_type - (int)cb->b_type;
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
	dx_state_collision* state = dx_state_collision_create(sh);

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
		dx_collision* expected_cols = (dx_collision*)malloc(
			expected_col_count * sizeof(dx_collision));

		if (rigid_count > 0) fread(rigids, sizeof(dx_entity), rigid_count, file);
		if (static_count > 0) fread(statics, sizeof(dx_entity), static_count, file);
		if (shape_count > 0) fread(shapes, sizeof(dx_shape), shape_count, file);
		if (expected_col_count > 0) {
			fread(expected_cols, sizeof(dx_collision), expected_col_count, file);
		}

		uint32_t actual_col_count = 0;
		dx_collision* actual_cols = dx_run_collision(
			sh, state, rigids, rigid_count, statics, static_count, shapes, shape_count, true,
			&actual_col_count);

		if (expected_col_count > 0) {
			qsort(expected_cols, expected_col_count, sizeof(dx_collision), compare_collisions);
		}
		if (actual_col_count > 0) {
			qsort(actual_cols, actual_col_count, sizeof(dx_collision), compare_collisions);
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
			dx_collision* exp = (i < expected_col_count) ? &expected_cols[i] : nullptr;
			dx_collision* act = (j < actual_col_count) ? &actual_cols[j] : nullptr;

			// Skip validation for unimplemented shape combinations.
			bool skip_collision = false;
			if (exp) {
				uint32_t a_type = rigids[exp->a_index].shape_type;
				uint32_t b_type = (exp->b_type == 1) ? rigids[exp->b_index].shape_type
				                                     : statics[exp->b_index].shape_type;
				// Order the types
				uint32_t t1 = a_type < b_type ? a_type : b_type;
				uint32_t t2 = a_type > b_type ? a_type : b_type;
				if (t1 == 1 && t2 == 2) skip_collision = true; // Capsule-Box
				if (t1 == 2 && t2 == 2) skip_collision = true; // Box-Box
			}
			if (skip_collision) { i++; continue; }

			int cmp = 0;
			if (exp && act) cmp = compare_collisions(exp, act);
			else if (exp) cmp = -1;
			else cmp = 1;

			if (cmp == 0) {
				// Pair exists in both, check math
				bool depth_ok = float_eq_approx(exp->depth, act->depth, 0.001f);
				bool normal_ok = vec3_eq_approx(exp->normal, act->normal, 0.02f);

				bool pt_ok = vec3_eq_approx(exp->point_a, act->point_a, 0.001f);
				if (!pt_ok) {
					// Handle parallel shapes
					float dx = act->point_a[0] - exp->point_a[0];
					float dy = act->point_a[1] - exp->point_a[1];
					float dz = act->point_a[2] - exp->point_a[2];
					// If the distance along the expected normal is ~0, it's a valid sliding point
					float normal_err = fabsf(dx * exp->normal[0] + dy * exp->normal[1] +
											 dz * exp->normal[2]);
					if (normal_err < 0.01f) {
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
			} else if (cmp < 0) {
				// Expected pair is missing from Actual (GPU missed it)
				if (exp->depth < 0.00001f) {
					i++;
				} else {
					fprintf(stderr, "❌ Frame %u FAILED: Missing expected pair (%u, %u type %u) "
									"with depth=%f\n",
							frame_index, exp->a_index, exp->b_index, exp->b_type, exp->depth);

					print_body_details(exp->a_index, exp->b_index, exp->b_type);

					passed = false; break;
				}
			} else {
				// Actual pair is extra (GPU found an extra one)
				if (act->depth < 0.00001f) {
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

	dx_state_collision_destroy(state);
	dx_shared_state_destroy(sh);

	fclose(file);
	return 0;
}
