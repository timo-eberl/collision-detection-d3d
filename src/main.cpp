#include "collision_dx.h"
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
	uint32_t counts[3];

	while (fread(counts, sizeof(uint32_t), 3, file) == 3) {
		uint32_t rigid_count = counts[0];
		uint32_t static_count = counts[1];
		uint32_t expected_col_count = counts[2];

		dx_shape* rigids = (dx_shape*)malloc(rigid_count * sizeof(dx_shape));
		dx_shape* statics = (dx_shape*)malloc(static_count * sizeof(dx_shape));
		dx_collision* expected_cols = (dx_collision*)malloc(expected_col_count * sizeof(dx_collision));

		if (rigid_count > 0) fread(rigids, sizeof(dx_shape), rigid_count, file);
		if (static_count > 0) fread(statics, sizeof(dx_shape), static_count, file);
		if (expected_col_count > 0) fread(expected_cols, sizeof(dx_collision), expected_col_count, file);

		uint32_t actual_col_count = 0;
		dx_collision* actual_cols = dx_run_collision(
			sh, state, rigids, rigid_count, statics, static_count, true, &actual_col_count);

		if (expected_col_count > 0) {
			qsort(expected_cols, expected_col_count, sizeof(dx_collision), compare_collisions);
		}
		if (actual_col_count > 0) {
			qsort(actual_cols, actual_col_count, sizeof(dx_collision), compare_collisions);
		}

		bool passed = true;
		if (actual_col_count != expected_col_count) {
			fprintf(stderr, "❌ Frame %u FAILED: Expected %u collisions, got %u\n",
					frame_index, expected_col_count, actual_col_count);
			passed = false;
		} else {
			for (uint32_t i = 0; i < expected_col_count; ++i) {
				dx_collision* exp = &expected_cols[i];
				dx_collision* act = &actual_cols[i];

				if (exp->a_index != act->a_index || exp->b_index != act->b_index || exp->b_type != act->b_type) {
					fprintf(stderr, "❌ Frame %u FAILED: Pair mismatch at sorted index %u\n", frame_index, i);
					passed = false; break;
				}

				if (!float_eq_approx(exp->depth, act->depth) ||
					!vec3_eq_approx(exp->normal, act->normal) ||
					!vec3_eq_approx(exp->point_a, act->point_a)) {
					fprintf(stderr, "❌ Frame %u FAILED: Math mismatch for pair (%u, %u type %u)\n",
							frame_index, exp->a_index, exp->b_index, exp->b_type);
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
		free(expected_cols);
		if (actual_cols) free(actual_cols);

		frame_index++;
	}

	dx_state_collision_destroy(state);
	dx_shared_state_destroy(sh);

	fclose(file);
	return 0;
}
