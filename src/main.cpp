#include "collision_detection_d3d.h"
#include <cassert>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#define DEFERRED_VALIDATION 1
#define WARMUP_FRAME_START 150
#define WARMUP_FRAME_END 159
#define BENCHMARK_FRAME_START 160
#define BENCHMARK_FRAME_END 169

#define ENABLE_ALGO_NAIVE 1
#define ENABLE_ALGO_BINNED 1
#define ENABLE_ALGO_INDIRECT 1
#define ENABLE_ALGO_WORK_GRAPHS 1

#ifndef PROJECT_ROOT_DIR
#define PROJECT_ROOT_DIR "."
#endif

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

struct recorded_frame {
	uint32_t frame_index;
	uint32_t rigid_count;
	uint32_t static_count;
	uint32_t shape_count;
	uint32_t expected_col_count;

	dx_entity* rigids;
	dx_entity* statics;
	dx_shape* shapes;
	dx_collision_full* expected_cols;

	uint32_t naive_col_count;
	dx_collision_compact* naive_compact;

	uint32_t binned_col_count;
	dx_collision_compact* binned_compact;

	uint32_t indirect_col_count;
	dx_collision_compact* indirect_compact;

	uint32_t wg_col_count;
	dx_collision_compact* wg_compact;
};

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

bool verify_results(const char* algorithm_name,
					const dx_collision_compact* actual_compact, uint32_t actual_col_count,
					const dx_collision_full* expected_cols, uint32_t expected_col_count,
					const dx_entity* rigids, uint32_t rigid_count,
					const dx_entity* statics, const dx_shape* shapes,
					uint32_t frame_index) {

	dx_collision_full* actual_cols = nullptr;

	if (actual_col_count > 0) {
		actual_cols = (dx_collision_full*)malloc(actual_col_count * sizeof(dx_collision_full));

		for (uint32_t j = 0; j < actual_col_count; ++j) {
			const dx_collision_compact* comp = &actual_compact[j];
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

		qsort(actual_cols, actual_col_count, sizeof(dx_collision_full), compare_collisions);
	}

	auto print_body_details = [&](uint32_t a_idx, uint32_t b_idx, uint32_t b_type) {
		const dx_entity& a = rigids[a_idx];
		const dx_entity& b = (b_type == 1) ? rigids[b_idx] : statics[b_idx];
		const dx_shape& s_a = shapes[a.shape_index];
		const dx_shape& s_b = shapes[b.shape_index];

		const char* type_names[] = {"Sphere", "Capsule", "Box", "Convex"};

		printf("  Body A (%s):\n", a.shape_type < 4 ? type_names[a.shape_type] : "Unknown");
		printf("    Pos:  (%f, %f, %f)\n", a.position[0], a.position[1], a.position[2]);
		printf("    Rot:  (%f, %f, %f, %f)\n",
			   a.rotation[0], a.rotation[1], a.rotation[2], a.rotation[3]);
		printf("    Data: (%f, %f, %f, %f)\n", s_a.data[0], s_a.data[1], s_a.data[2], s_a.data[3]);

		printf("  Body B (%s):\n", b.shape_type < 4 ? type_names[b.shape_type] : "Unknown");
		printf("    Pos:  (%f, %f, %f)\n", b.position[0], b.position[1], b.position[2]);
		printf("    Rot:  (%f, %f, %f, %f)\n",
			   b.rotation[0], b.rotation[1], b.rotation[2], b.rotation[3]);
		printf("    Data: (%f, %f, %f, %f)\n", s_b.data[0], s_b.data[1], s_b.data[2], s_b.data[3]);
	};

	bool passed = true;
	uint32_t i = 0, j = 0;

	// walk through both arrays simultaneously
	// If we find a pair that exists in one list but not the other, we check its depth. If the
	// depth is close to zero, we ignore it
	while (i < expected_col_count || j < actual_col_count) {
		const dx_collision_full* exp = (i < expected_col_count) ? &expected_cols[i] : nullptr;
		const dx_collision_full* act = (j < actual_col_count) ? &actual_cols[j] : nullptr;

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
				printf("❌ Frame %u FAILED (%s): Math mismatch for pair (%u, %u type %u)\n",
					   frame_index, algorithm_name, exp->a_index, exp->b_index, exp->b_type);

				float exp_len = sqrtf(exp->normal[0] * exp->normal[0] +
									  exp->normal[1] * exp->normal[1] +
									  exp->normal[2] * exp->normal[2]);
				float act_len = sqrtf(act->normal[0] * act->normal[0] +
									  act->normal[1] * act->normal[1] +
									  act->normal[2] * act->normal[2]);

				printf("  Expected: depth=%f, normal=(%f, %f, %f) len=%f, pt_a=(%f, %f, %f)\n",
					   exp->depth, exp->normal[0], exp->normal[1], exp->normal[2], exp_len,
					   exp->point_a[0], exp->point_a[1], exp->point_a[2]);

				printf("  Actual:   depth=%f, normal=(%f, %f, %f) len=%f, pt_a=(%f, %f, %f)\n",
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
				printf("❌ Frame %u FAILED (%s): Missing expected pair (%u, %u type %u) "
					   "with depth=%f\n",
					   frame_index, algorithm_name, exp->a_index, exp->b_index,
					   exp->b_type, exp->depth);

				print_body_details(exp->a_index, exp->b_index, exp->b_type);

				passed = false; break;
			}
		}
		else {
			// Actual pair is extra (GPU found an extra one)
			if (act->depth < 0.0001f) {
				j++;
			} else {
				printf("❌ Frame %u FAILED (%s): Extra GPU pair (%u, %u type %u) with depth=%f\n",
					   frame_index, algorithm_name, act->a_index, act->b_index,
					   act->b_type, act->depth);

				print_body_details(act->a_index, act->b_index, act->b_type);

				passed = false; break;
			}
		}
	}

	if (actual_cols) free(actual_cols);
	return passed;
}

dx_collision_compact* deep_copy_compact(const dx_collision_compact* src, uint32_t count) {
	if (count == 0 || !src) return nullptr;
	dx_collision_compact* dst = (dx_collision_compact*)malloc(count * sizeof(dx_collision_compact));
	if (dst) memcpy(dst, src, count * sizeof(dx_collision_compact));
	return dst;
}

int main() {

	// Fix emojis on some Windows terminals
#ifdef _WIN32
	SetConsoleOutputCP(CP_UTF8);
#endif

	FILE* file = fopen(PROJECT_ROOT_DIR "/collision_test_data.bin", "rb");
	if (!file) {
		printf("❌ Failed to open " PROJECT_ROOT_DIR "/collision_test_data.bin\n");
		return 1;
	}

	dx_shared_state* sh = dx_shared_state_create();
	dx_state_simple_naive* state_naive = nullptr;
	dx_state_simple_binned* state_binned = nullptr;
	dx_state_execute_indirect* state_indirect = nullptr;
	dx_state_work_graphs* state_work_graphs = nullptr;

	if (ENABLE_ALGO_NAIVE) state_naive = dx_state_simple_naive_create(sh);
	if (ENABLE_ALGO_BINNED) state_binned = dx_state_simple_binned_create(sh);
	if (ENABLE_ALGO_INDIRECT) state_indirect = dx_state_execute_indirect_create(sh);
	if (ENABLE_ALGO_WORK_GRAPHS) state_work_graphs = dx_state_work_graphs_create(sh);

	uint32_t frame_index = 0;
	uint32_t counts[4];
	
	std::vector<recorded_frame> recorded_frames;
	uint32_t max_required_frame = WARMUP_FRAME_END > BENCHMARK_FRAME_END ? 
								  WARMUP_FRAME_END : BENCHMARK_FRAME_END;

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

		bool is_warmup = (frame_index >= WARMUP_FRAME_START && frame_index <= WARMUP_FRAME_END);
		bool is_benchmark = (frame_index >= BENCHMARK_FRAME_START && 
							 frame_index <= BENCHMARK_FRAME_END);

		assert(WARMUP_FRAME_END < BENCHMARK_FRAME_START);

		if (!is_warmup && !is_benchmark) {
			free(rigids);
			free(statics);
			free(shapes);
			free(expected_cols);
			
			if (frame_index > max_required_frame) break;
			
			frame_index++;
			continue;
		}

		if (frame_index == WARMUP_FRAME_START) {
			printf("Warmup (Frames %u - %u)...\n", WARMUP_FRAME_START, WARMUP_FRAME_END);
		}
		if (frame_index == BENCHMARK_FRAME_START) {
			printf("\n--- Benchmarking (Frames %u - %u) ---\n",
				   BENCHMARK_FRAME_START, BENCHMARK_FRAME_END);
		}

		// Define the grid configuration based on the scene bounds + padding
		dx_grid_config grid_config = {
			.res_x = 20,
			.res_y = 20,
			.res_z = 34,
			.origin_x = -50.0f,
			.origin_y = -50.0f,
			.origin_z = -85.0f,
			.cell_size = 5.0f
		};

		dx_shared_state_set_profiling(sh, is_benchmark);

		uint32_t naive_col_count = 0;
		dx_collision_compact* naive_compact = nullptr;
		if (ENABLE_ALGO_NAIVE) {
			naive_compact = dx_run_simple_naive(sh, state_naive, &grid_config, rigids,
												rigid_count, statics, static_count, shapes,
												shape_count, true, &naive_col_count);
		}

		uint32_t binned_col_count = 0;
		dx_collision_compact* binned_compact = nullptr;
		if (ENABLE_ALGO_BINNED) {
			binned_compact = dx_run_simple_binned(sh, state_binned, &grid_config, rigids,
												  rigid_count, statics, static_count, shapes,
												  shape_count, true, &binned_col_count);
		}

		uint32_t indirect_col_count = 0;
		dx_collision_compact* indirect_compact = nullptr;
		if (ENABLE_ALGO_INDIRECT) {
			indirect_compact = dx_run_execute_indirect(sh, state_indirect, &grid_config, rigids,
													   rigid_count, statics, static_count, shapes,
													   shape_count, true, &indirect_col_count);
		}

		uint32_t wg_col_count = 0;
		dx_collision_compact* wg_compact = nullptr;
		if (ENABLE_ALGO_WORK_GRAPHS) {
			wg_compact = dx_run_work_graphs(sh, state_work_graphs, &grid_config, rigids,
											rigid_count, statics, static_count, shapes,
											shape_count, true, &wg_col_count);
		}

		if (is_benchmark && DEFERRED_VALIDATION) {
			recorded_frame rec = {};
			rec.frame_index = frame_index;
			rec.rigid_count = rigid_count;
			rec.static_count = static_count;
			rec.shape_count = shape_count;
			rec.expected_col_count = expected_col_count;
			
			// Transfer ownership of the dynamically allocated arrays to the struct
			rec.rigids = rigids;
			rec.statics = statics;
			rec.shapes = shapes;
			rec.expected_cols = expected_cols;
			
			rec.naive_col_count = naive_col_count;
			rec.naive_compact = deep_copy_compact(naive_compact, naive_col_count);
			
			rec.binned_col_count = binned_col_count;
			rec.binned_compact = deep_copy_compact(binned_compact, binned_col_count);
			
			rec.indirect_col_count = indirect_col_count;
			rec.indirect_compact = deep_copy_compact(indirect_compact, indirect_col_count);
			
			rec.wg_col_count = wg_col_count;
			rec.wg_compact = deep_copy_compact(wg_compact, wg_col_count);
			
			recorded_frames.push_back(rec);
		} else {
			free(rigids);
			free(statics);
			free(shapes);
			free(expected_cols);
		}

		frame_index++;
	}

	if (DEFERRED_VALIDATION && !recorded_frames.empty()) {
		printf("\n--- Deferred Validation ---\n");

		for (size_t k = 0; k < recorded_frames.size(); ++k) {
			recorded_frame& rec = recorded_frames[k];
			
			if (rec.expected_col_count > 0) {
				qsort(rec.expected_cols, rec.expected_col_count, sizeof(dx_collision_full), 
					  compare_collisions);
			}

			bool passed_all = true;

			if (ENABLE_ALGO_NAIVE) {
				passed_all &= verify_results("Naive", rec.naive_compact, rec.naive_col_count,
											 rec.expected_cols, rec.expected_col_count, rec.rigids,
											 rec.rigid_count, rec.statics, rec.shapes,
											 rec.frame_index);
			}

			if (ENABLE_ALGO_BINNED) {
				passed_all &= verify_results("Binned", rec.binned_compact, rec.binned_col_count,
											 rec.expected_cols, rec.expected_col_count, rec.rigids,
											 rec.rigid_count, rec.statics, rec.shapes,
											 rec.frame_index);
			}

			if (ENABLE_ALGO_INDIRECT) {
				passed_all &= verify_results("Indirect", rec.indirect_compact,
											 rec.indirect_col_count, rec.expected_cols,
											 rec.expected_col_count, rec.rigids, rec.rigid_count,
											 rec.statics, rec.shapes, rec.frame_index);
			}

			if (ENABLE_ALGO_WORK_GRAPHS) {
				passed_all &= verify_results("Work Graphs", rec.wg_compact, rec.wg_col_count,
											 rec.expected_cols, rec.expected_col_count, rec.rigids,
											 rec.rigid_count, rec.statics, rec.shapes,
											 rec.frame_index);
			}

			if (passed_all) {
				printf("✅ Frame %u PASSED: %u collisions\n",
					   rec.frame_index, rec.expected_col_count);
			}

			free(rec.rigids);
			free(rec.statics);
			free(rec.shapes);
			free(rec.expected_cols);
			free(rec.naive_compact);
			free(rec.binned_compact);
			free(rec.indirect_compact);
			free(rec.wg_compact);
		}
		
		// Flush, so stdout and stderr messages are printed in order (OS dependent)
		fflush(stdout);
		fflush(stderr);
	}

	dx_state_simple_naive_destroy(state_naive);
	dx_state_simple_binned_destroy(state_binned);
	dx_state_execute_indirect_destroy(state_indirect);
	dx_state_work_graphs_destroy(state_work_graphs);
	dx_shared_state_destroy(sh);

	fclose(file);
	return 0;
}
