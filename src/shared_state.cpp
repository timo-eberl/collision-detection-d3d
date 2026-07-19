#include "collision_dx.h"
#include "dx_shared.h"

extern "C" dx_shared_state* dx_shared_state_create(void) {
	dx_shared_state* s = (dx_shared_state*)calloc(1, sizeof(dx_shared_state));

	IDXCoreAdapterFactory* factory = nullptr;
	HRESULT hr = DXCoreCreateAdapterFactory(IID_PPV_ARGS(&factory));
	if (FAILED(hr)) {
		fprintf(stderr, "[dx12] Failed to create DXCore Adapter Factory.\n");
		return s;
	}

	IDXCoreAdapterList* adapter_list = nullptr;
	const GUID dx12_guid = DXCORE_ADAPTER_ATTRIBUTE_D3D12_GRAPHICS;
	hr = factory->CreateAdapterList(1, &dx12_guid, IID_PPV_ARGS(&adapter_list));
	if (FAILED(hr)) {
		fprintf(stderr, "[dx12] Failed to create DXCore Adapter List.\n");
		factory->Release();
		return s;
	}

	IDXCoreAdapter* adapter = nullptr;
	if (adapter_list->GetAdapterCount() > 0) {
		hr = adapter_list->GetAdapter(1, IID_PPV_ARGS(&adapter));
		if (SUCCEEDED(hr)) {
			ID3D12Device* base_device = nullptr;
			hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&base_device));
			
			if (SUCCEEDED(hr)) {
				// Query the ID3D12Device10 interface to access Enhanced Barriers Create API
				hr = base_device->QueryInterface(IID_PPV_ARGS(&s->device));
				base_device->Release();
				
				if (SUCCEEDED(hr)) {
					char desc[128] = {0};
					hr = adapter->GetProperty(DXCoreAdapterProperty::DriverDescription,
											  sizeof(desc), desc);
					fprintf(stderr, "[dx12] Initialized D3D12 Device on: %s\n",
							SUCCEEDED(hr) ? desc : "unknown adapter");
				}
			}
		}
	} else {
		fprintf(stderr, "[dx12] No DX12 compatible adapters found.\n");
	}
	
	if (adapter) adapter->Release();
	if (adapter_list) adapter_list->Release();
	if (factory) factory->Release();

	if (!s->device) {
		fprintf(stderr, "[dx12] Failed to create D3D12 Device10.\n");
		return s;
	}

	D3D12_COMMAND_QUEUE_DESC q_desc = {};
	q_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	DX_CHECK(s->device->CreateCommandQueue(&q_desc, IID_PPV_ARGS(&s->cmd_queue)));
	s->cmd_queue->SetName(L"Physics_Compute_Queue");

	DX_CHECK(s->device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&s->cmd_allocator)));

	DX_CHECK(s->device->CreateCommandList(
		0, D3D12_COMMAND_LIST_TYPE_COMPUTE, s->cmd_allocator, nullptr,
		IID_PPV_ARGS(&s->cmd_list)));
	s->cmd_list->SetName(L"Physics_Command_List");

	// Setup Profiling Heaps
	DX_CHECK(s->cmd_queue->GetTimestampFrequency(&s->timestamp_frequency));
	D3D12_QUERY_HEAP_DESC qh_desc = {};
	qh_desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	qh_desc.Count = 32;
	DX_CHECK(s->device->CreateQueryHeap(&qh_desc, IID_PPV_ARGS(&s->query_heap)));
	ensure_dx_buffer(s->device, &s->rb_query, &s->rb_query_size, 32, sizeof(uint64_t),
					 D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, 1.0f);

	DX_CHECK(s->device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&s->fence)));
	s->fence_value = 0;
	s->fence_event = dx_create_event();

	// Allocate a persistent 4-byte buffer containing 0 to quickly reset atomic counters
	ensure_dx_buffer(s->device, &s->up_zero, &s->up_zero_size, 1, sizeof(uint32_t),
					 D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_FLAG_NONE, 1.0f);
	void* p_zero;
	s->up_zero->Map(0, nullptr, &p_zero);
	*(uint32_t*)p_zero = 0;
	s->up_zero->Unmap(0, nullptr);
	
	return s;
}

extern "C" void dx_shared_state_destroy(dx_shared_state* s) {
	if (!s) return;
	
	if (s->cmd_queue && s->fence && s->fence_event) {
		s->fence_value++;
		s->cmd_queue->Signal(s->fence, s->fence_value);
		if (s->fence->GetCompletedValue() < s->fence_value) {
			s->fence->SetEventOnCompletion(s->fence_value, (HANDLE)s->fence_event);
			dx_wait_event(s->fence_event);
		}
	}

	if (s->up_rigids) s->up_rigids->Release();
	if (s->up_statics) s->up_statics->Release();
	if (s->d_rigids) s->d_rigids->Release();
	if (s->d_statics) s->d_statics->Release();
	if (s->d_collisions) s->d_collisions->Release();
	if (s->d_col_count) s->d_col_count->Release();
	if (s->rb_collisions) s->rb_collisions->Release();
	if (s->rb_col_count) s->rb_col_count->Release();

	if (s->rb_query) s->rb_query->Release();
	if (s->query_heap) s->query_heap->Release();
	if (s->up_zero) s->up_zero->Release();

	if (s->fence_event) dx_close_event(s->fence_event);
	if (s->fence) s->fence->Release();
	if (s->cmd_list) s->cmd_list->Release();
	if (s->cmd_allocator) s->cmd_allocator->Release();
	if (s->cmd_queue) s->cmd_queue->Release();
	if (s->device) s->device->Release();
	
	free(s);
}

extern "C" void dx_shared_ensure_buffers(dx_shared_state* sh, uint32_t rigid_count,
										 uint32_t static_count, size_t max_collisions) {
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
					 sizeof(dx_collision), D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_FLAG_NONE, 1.0f);

	if (sh->d_rigids) sh->d_rigids->SetName(L"Rigids_Default_Buffer");
	if (sh->d_statics) sh->d_statics->SetName(L"Statics_Default_Buffer");
	if (sh->d_collisions) sh->d_collisions->SetName(L"Collisions_UAV");
}

extern "C" void dx_shared_write_inputs(ID3D12Resource* up_rigids, const dx_shape* rigids,
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

extern "C" uint32_t dx_shared_read_count(ID3D12Resource* rb_col_count) {
	uint32_t count = 0;
	void* mapped = nullptr;
	rb_col_count->Map(0, nullptr, &mapped);
	count = *(uint32_t*)mapped;
	rb_col_count->Unmap(0, nullptr);
	return count;
}

extern "C" dx_collision* dx_shared_read_collisions(ID3D12Resource* rb_collisions, uint32_t count) {
	dx_collision* h_cols = (dx_collision*)malloc(count * sizeof(dx_collision));
	void* mapped = nullptr;
	rb_collisions->Map(0, nullptr, &mapped);
	memcpy(h_cols, mapped, count * sizeof(dx_collision));
	rb_collisions->Unmap(0, nullptr);
	return h_cols;
}
