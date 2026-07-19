#ifndef SHARED_H
#define SHARED_H

#include "collision_detection_d3d.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
	#include <windows.h>
#else
	#include <wsl/winadapter.h>
	#include <sys/eventfd.h>
	#include <unistd.h>
	#include <poll.h>
#endif

#include <directx/d3d12.h>
#include <directx/dxcore.h>
#include <dxguids/dxguids.h>

// Architecture Notes
//
// Shared Layer (shared.h / shared_state.cpp):
// Owns the D3D12 device, queue, and memory allocations. Responsible solely for sizing buffers and
// CPU-side memory mapping. The shared layer does not record commands, issue barriers or interact
// with the command list.
//
// Variant Layer (algo_simple.cpp, ...):
// Owns the Root Signatures, PSOs, and the entire GPU execution timeline. The variant implementation
// is responsible for recording all GPU-side copies, explicit state barriers, dispatches, profiling
// steps, and explicitly flushing the command queue via execute_and_wait.

// Represents the hardware context and universal data required by all algorithms.
struct dx_shared_state {
	ID3D12Device10* device;
	ID3D12CommandQueue* cmd_queue;
	ID3D12CommandAllocator* cmd_allocator;
	ID3D12GraphicsCommandList7* cmd_list;

	ID3D12Fence* fence;
	uint64_t fence_value;
	void* fence_event;

	// Input Upload Buffersy (on CPU, GPU writable)
	ID3D12Resource* up_rigids;
	size_t up_rigids_size;
	ID3D12Resource* up_statics;
	size_t up_statics_size;

	// Default Buffers (GPU Only)
	ID3D12Resource* d_rigids;
	size_t d_rigids_size;
	ID3D12Resource* d_statics;
	size_t d_statics_size;
	ID3D12Resource* d_collisions;
	size_t d_collisions_size;
	ID3D12Resource* d_col_count;
	size_t d_col_count_size;

	// Readback Buffers (on CPU, GPU readable)
	ID3D12Resource* rb_collisions;
	size_t rb_collisions_size;
	ID3D12Resource* rb_col_count;
	size_t rb_col_count_size;

	// Profiling
	uint64_t timestamp_frequency;
	ID3D12QueryHeap* query_heap;
	ID3D12Resource* rb_query;
	size_t rb_query_size;

	// Utility Buffers
	ID3D12Resource* up_zero;
	size_t up_zero_size;
};

#define DX_CHECK(call) \
	do { \
		HRESULT hr_ = (call); \
		if (FAILED(hr_)) { \
			fprintf(stderr, "[dx12] %s:%d HRESULT 0x%08X\n", __FILE__, __LINE__, \
					(unsigned int)hr_); \
		} \
	} while (0)

#ifdef _WIN32
	static inline void* dx_create_event(void) {
		return CreateEventA(nullptr, FALSE, FALSE, nullptr);
	}
	static inline void dx_close_event(void* handle) {
		if (handle) CloseHandle((HANDLE)handle);
	}
	static inline void dx_wait_event(void* handle) {
		WaitForSingleObject((HANDLE)handle, INFINITE);
	}
#else
	static inline void* dx_create_event(void) {
		int fd = eventfd(0, 0);
		if (fd < 0) return nullptr;
		return (void*)(uintptr_t)fd;
	}
	static inline void dx_close_event(void* handle) {
		int fd = (int)(uintptr_t)handle;
		if (fd >= 0) close(fd);
	}
	static inline void dx_wait_event(void* handle) {
		int fd = (int)(uintptr_t)handle;
		if (fd < 0) return;
		struct pollfd pfd = {};
		pfd.fd = fd;
		pfd.events = POLLIN;
		poll(&pfd, 1, -1);
		uint64_t val = 0;
		ssize_t r = read(fd, &val, sizeof(val));
		(void)r;
	}
#endif

static inline void ensure_dx_buffer(ID3D12Device10* device, ID3D12Resource** d_buf,
									size_t* capacity, size_t needed, size_t elem_size,
									D3D12_HEAP_TYPE heap_type, D3D12_RESOURCE_FLAGS flags,
									float growth_factor) {
	if (*capacity >= needed) return;
	if (*d_buf) {
		(*d_buf)->Release();
		*d_buf = nullptr;
	}

	size_t target_capacity = (size_t)(needed * growth_factor);
	if (target_capacity < needed) {
		target_capacity = needed;
	}
	*capacity = 0;

	D3D12_HEAP_PROPERTIES heap_props = {};
	heap_props.Type = heap_type;
	heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_props.CreationNodeMask = 1;
	heap_props.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC1 desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = 0;
	desc.Width = target_capacity * elem_size;
	desc.Height = 1;
	desc.DepthOrArraySize = 1;
	desc.MipLevels = 1;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = flags;

	// Buffers physically have no layout, so we use UNDEFINED.
	HRESULT hr = device->CreateCommittedResource3(
		&heap_props, D3D12_HEAP_FLAG_NONE, &desc, D3D12_BARRIER_LAYOUT_UNDEFINED,
		nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(d_buf));

	if (SUCCEEDED(hr)) {
		*capacity = target_capacity;
	} else {
		fprintf(stderr, "[dx12] Failed to allocate buffer of size %zu\n",
				target_capacity * elem_size);
	}
}

static inline void execute_and_wait(dx_shared_state* sh) {
	DX_CHECK(sh->cmd_list->Close());
	ID3D12CommandList* lists[] = { sh->cmd_list };
	sh->cmd_queue->ExecuteCommandLists(1, lists);

	sh->fence_value++;
	DX_CHECK(sh->cmd_queue->Signal(sh->fence, sh->fence_value));

	if (sh->fence->GetCompletedValue() < sh->fence_value) {
		DX_CHECK(sh->fence->SetEventOnCompletion(sh->fence_value, (HANDLE)sh->fence_event));
		dx_wait_event(sh->fence_event);
	}

	DX_CHECK(sh->cmd_allocator->Reset());
	DX_CHECK(sh->cmd_list->Reset(sh->cmd_allocator, nullptr));
}

// Allocates/resizes all necessary D3D12 buffers (Upload, Default, and Readback) based on capacities
static void shared_ensure_buffers(dx_shared_state* sh, uint32_t rigid_count, uint32_t static_count,
								  size_t max_collisions) {
	ensure_dx_buffer(sh->device, &sh->up_rigids, &sh->up_rigids_size, rigid_count, sizeof(dx_shape),
					 D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, 1.0f);
	ensure_dx_buffer(sh->device, &sh->d_rigids, &sh->d_rigids_size, rigid_count, sizeof(dx_shape),
					 D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_NONE, 1.0f);

	if (static_count > 0) {
		ensure_dx_buffer(sh->device, &sh->up_statics, &sh->up_statics_size, static_count,
						 sizeof(dx_shape), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, 1.0f);
		ensure_dx_buffer(sh->device, &sh->d_statics, &sh->d_statics_size, static_count,
						 sizeof(dx_shape), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_NONE, 1.0f);
	}

	ensure_dx_buffer(sh->device, &sh->d_col_count, &sh->d_col_count_size, 1, sizeof(uint32_t),
					 D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	ensure_dx_buffer(sh->device, &sh->rb_col_count, &sh->rb_col_count_size, 1, sizeof(uint32_t),
					 D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, 1.0f);

	ensure_dx_buffer(sh->device, &sh->d_collisions, &sh->d_collisions_size, max_collisions,
					 sizeof(dx_collision), D3D12_HEAP_TYPE_DEFAULT,
					 D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, 1.0f);
	// Allocate the readback buffer for the same capacity upfront. Could be done later with only the
	// actually required size, but who cares about memory anymore, right?
	ensure_dx_buffer(sh->device, &sh->rb_collisions, &sh->rb_collisions_size, max_collisions,
					 sizeof(dx_collision), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE,1.0f);

	if (sh->d_rigids) sh->d_rigids->SetName(L"Rigids_Default_Buffer");
	if (sh->d_statics) sh->d_statics->SetName(L"Statics_Default_Buffer");
	if (sh->d_collisions) sh->d_collisions->SetName(L"Collisions_UAV");
}

// Maps the upload buffers and copies the host shape data into them.
// Takes only the specific resources it operates on.
static void shared_write_inputs(ID3D12Resource* up_rigids, const dx_shape* rigids,
								uint32_t rigid_count, ID3D12Resource* up_statics,
								const dx_shape* statics, uint32_t static_count,
								bool statics_changed) {
	void* mapped = nullptr;
	up_rigids->Map(0, nullptr, &mapped);
	memcpy(mapped, rigids, rigid_count * sizeof(dx_shape));
	up_rigids->Unmap(0, nullptr);

	if (statics_changed && static_count > 0) {
		up_statics->Map(0, nullptr, &mapped);
		memcpy(mapped, statics, static_count * sizeof(dx_shape));
		up_statics->Unmap(0, nullptr);
	}
}

// Maps the readback buffer and extracts the final collision count.
static uint32_t shared_read_count(ID3D12Resource* rb_col_count) {
	uint32_t count = 0;
	void* mapped = nullptr;
	rb_col_count->Map(0, nullptr, &mapped);
	count = *(uint32_t*)mapped;
	rb_col_count->Unmap(0, nullptr);
	return count;
}

// Maps the readback buffer, allocates a host array, and copies the final collision structs.
static dx_collision* shared_read_collisions(ID3D12Resource* rb_collisions, uint32_t count) {
	dx_collision* h_cols = (dx_collision*)malloc(count * sizeof(dx_collision));
	void* mapped = nullptr;
	rb_collisions->Map(0, nullptr, &mapped);
	memcpy(h_cols, mapped, count * sizeof(dx_collision));
	rb_collisions->Unmap(0, nullptr);
	return h_cols;
}

#endif
