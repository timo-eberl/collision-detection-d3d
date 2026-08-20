#ifndef DX_PROFILE_H
#define DX_PROFILE_H

#include "shared.h"
#include <chrono>
#include <float.h>
#include <string.h>

#ifdef CDDX_ENABLE_PROFILER

#define DX_PROFILE_MAX_STEPS 16
#define DX_PROFILE_MAX_CPU_STEPS 16

typedef struct {
	const char* labels[DX_PROFILE_MAX_STEPS];
	int start_queries[DX_PROFILE_MAX_STEPS];
	int end_queries[DX_PROFILE_MAX_STEPS];
	float intervals[DX_PROFILE_MAX_STEPS];
	int count;
	int query_count;

	const char* cpu_labels[DX_PROFILE_MAX_CPU_STEPS];
	float cpu_intervals[DX_PROFILE_MAX_CPU_STEPS];
	int cpu_count;
	std::chrono::high_resolution_clock::time_point last_cpu_time;
} dx_profile;

typedef struct {
	float acc[DX_PROFILE_MAX_STEPS];
	float min[DX_PROFILE_MAX_STEPS];
	float max[DX_PROFILE_MAX_STEPS];
	int count;

	float cpu_acc[DX_PROFILE_MAX_CPU_STEPS];
	float cpu_min[DX_PROFILE_MAX_CPU_STEPS];
	float cpu_max[DX_PROFILE_MAX_CPU_STEPS];
	int cpu_count;

	int calls;
	float total_acc;
	float total_min;
	float total_max;
} dx_profile_acc;

static inline void dx_profile_acc_init(dx_profile_acc* a) {
	memset(a, 0, sizeof(*a));
	for (int i = 0; i < DX_PROFILE_MAX_STEPS; ++i) {
		a->min[i] = FLT_MAX;
		a->max[i] = -FLT_MAX;
	}
	for (int i = 0; i < DX_PROFILE_MAX_CPU_STEPS; ++i) {
		a->cpu_min[i] = FLT_MAX;
		a->cpu_max[i] = -FLT_MAX;
	}
	a->total_min = FLT_MAX;
	a->total_max = -FLT_MAX;
}

static inline void dx_profile_cpu_begin(dx_profile* p, dx_shared_state* sh) {
	p->cpu_count = 0;
	if (sh->disable_profiling) return;
	p->last_cpu_time = std::chrono::high_resolution_clock::now();
}

static inline void dx_profile_cpu_step(dx_profile* p, const char* label) {
	if (p->cpu_count >= DX_PROFILE_MAX_CPU_STEPS) return;

	auto now = std::chrono::high_resolution_clock::now();
	std::chrono::duration<float, std::milli> duration = now - p->last_cpu_time;

	int i = p->cpu_count;
	p->cpu_labels[i] = label;
	p->cpu_intervals[i] = duration.count();
	p->cpu_count++;

	p->last_cpu_time = now;
}

static inline void dx_profile_begin(dx_profile* p, dx_shared_state* sh) {
	p->count = 0;
	if (sh->disable_profiling) return;

	p->query_count = 1;

	// See the comment in dx_profile_step on why we immediately resolve
	sh->cmd_list->EndQuery(sh->query_heap, D3D12_QUERY_TYPE_TIMESTAMP, 0);
	sh->cmd_list->ResolveQueryData(sh->query_heap, D3D12_QUERY_TYPE_TIMESTAMP, 0, 1,
								   sh->rb_query, 0);
}

static inline void dx_profile_step(dx_profile* p, dx_shared_state* sh, const char* label) {
	if (sh->disable_profiling) return;
	if (p->count >= DX_PROFILE_MAX_STEPS) return;
	int i = p->count;
	p->labels[i] = label;
	p->start_queries[i] = p->query_count - 1;
	p->end_queries[i] = p->query_count;
	p->count++;

	sh->cmd_list->EndQuery(sh->query_heap, D3D12_QUERY_TYPE_TIMESTAMP, p->query_count);
	// The DX12 API is designed so that we usually wouldn't resolve every time we record a
	// timestamp. However the AMD implementation requires it for accurate results. The issue is
	// reproducible with 15000 objects and 200 steps (upload time sometimes has kernel time).
	sh->cmd_list->ResolveQueryData(sh->query_heap, D3D12_QUERY_TYPE_TIMESTAMP, p->query_count, 1,
								   sh->rb_query, p->query_count * sizeof(uint64_t));
	p->query_count++;
}

static inline void dx_profile_end(dx_profile* p, dx_shared_state* sh) {
	if (sh->disable_profiling) return;

	if (p->count == 0) return;

	uint64_t* timestamps = nullptr;
	D3D12_RANGE read_range = {0, (size_t)(p->query_count * sizeof(uint64_t))};
	sh->rb_query->Map(0, &read_range, (void**)&timestamps);

	for (int i = 0; i < p->count; ++i) {
		uint64_t start = timestamps[p->start_queries[i]];
		uint64_t end = timestamps[p->end_queries[i]];
		p->intervals[i] = (float)((end - start) * 1000.0 / (double)sh->timestamp_frequency);
	}

	D3D12_RANGE write_range = {0, 0};
	sh->rb_query->Unmap(0, &write_range);
}

static inline void dx_profile_log_frame(const dx_profile* p, const char* algo_label) {
	if (p->count == 0) return;

	fprintf(stderr, "[dx12] %s (frame)\n    GPU:", algo_label);
	float total = 0.0f;
	for (int i = 0; i < p->count; ++i) {
		total += p->intervals[i];
		fprintf(stderr, " %s=%.3fms", p->labels[i], p->intervals[i]);
	}
	fprintf(stderr, " total=%.3fms\n", total);

	if (p->cpu_count > 0) {
		fprintf(stderr, "    CPU:");
		for (int i = 0; i < p->cpu_count; ++i) {
			fprintf(stderr, " %s=%.3fms", p->cpu_labels[i], p->cpu_intervals[i]);
		}
		fprintf(stderr, "\n");
	}
}

static inline void dx_profile_log(const dx_profile* p, dx_profile_acc* a, const char* algo_label,
								  int every) {
	if (p->count == 0) return;

	if (a->calls == 0) {
		a->count = p->count;
		a->cpu_count = p->cpu_count;
	}
	if (p->count > a->count) a->count = p->count;
	if (p->cpu_count > a->cpu_count) a->cpu_count = p->cpu_count;

	float current_total = 0.0f;
	int n = p->count < a->count ? p->count : a->count;
	for (int i = 0; i < n; ++i) {
		float val = p->intervals[i];
		current_total += val;
		a->acc[i] += val;
		if (val < a->min[i]) a->min[i] = val;
		if (val > a->max[i]) a->max[i] = val;
	}

	int cn = p->cpu_count < a->cpu_count ? p->cpu_count : a->cpu_count;
	for (int i = 0; i < cn; ++i) {
		float val = p->cpu_intervals[i];
		a->cpu_acc[i] += val;
		if (val < a->cpu_min[i]) a->cpu_min[i] = val;
		if (val > a->cpu_max[i]) a->cpu_max[i] = val;
	}

	a->total_acc += current_total;
	if (current_total < a->total_min) a->total_min = current_total;
	if (current_total > a->total_max) a->total_max = current_total;

	a->calls++;

	if (a->calls % every != 0) return;

	fprintf(stderr, "[dx12] %s (avg over %d)\n    GPU:", algo_label, a->calls);
	for (int i = 0; i < n; ++i) {
		float avg = a->acc[i] / a->calls;
		fprintf(stderr, " %s=%.3fms [%.3f-%.3f]", p->labels[i], avg, a->min[i], a->max[i]);
	}

	float total_avg = a->total_acc / a->calls;
	fprintf(stderr, " total=%.3fms [%.3fms-%.3fms]\n",
			total_avg, a->total_min, a->total_max);

	if (cn > 0) {
		fprintf(stderr, "    CPU:");
		for (int i = 0; i < cn; ++i) {
			float avg = a->cpu_acc[i] / a->calls;
			fprintf(stderr, " %s=%.3fms", p->cpu_labels[i], avg);
		}
		fprintf(stderr, "\n");
	}
}

#else

typedef struct { int _dummy; } dx_profile;
typedef struct { int _dummy; } dx_profile_acc;

static inline void dx_profile_acc_init(dx_profile_acc*) {}
static inline void dx_profile_cpu_begin(dx_profile* p, dx_shared_state* sh) {}
static inline void dx_profile_cpu_step(dx_profile*, const char*) {}
static inline void dx_profile_begin(dx_profile*, dx_shared_state*) {}
static inline void dx_profile_step(dx_profile*, dx_shared_state*, const char*) {}
static inline void dx_profile_end(dx_profile*, dx_shared_state*) {}
static inline void dx_profile_log_frame(const dx_profile*, const char*) {}
static inline void dx_profile_log(const dx_profile*, dx_profile_acc*, const char*, int) {}

#endif // CDDX_ENABLE_PROFILER

#endif
