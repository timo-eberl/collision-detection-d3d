#ifndef COLLISION_DETECTION_D3D
#define COLLISION_DETECTION_D3D

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
	float position[3];
	uint32_t shape_type; // 0 = Sphere, 1 = Capsule, 2 = OBB
	float rotation[4];
	uint32_t shape_index;
	uint32_t pad[3];
} dx_entity; // 48 bytes

typedef union {
	struct { float radius; uint32_t pad[3]; } sphere;
	struct { float half_height; float radius; uint32_t pad[2]; } capsule;
	struct { float half_extents[3]; uint32_t pad; } obb;
	float data[4];
} dx_shape; // 16 bytes

typedef struct {
	uint32_t a_index;
	uint32_t b_index;
	float depth;
	float point_a[3];
	float normal[2];
} dx_collision_compact; // 32 bytes

typedef struct {
	int res_x, res_y, res_z;
	float origin_x, origin_y, origin_z;
	float cell_size;
} dx_grid_config;

typedef struct dx_shared_state dx_shared_state;
// Opaque state structs for each algorithm
typedef struct dx_state_simple_naive dx_state_simple_naive;
typedef struct dx_state_simple_binned dx_state_simple_binned;
typedef struct dx_state_execute_indirect dx_state_execute_indirect;
typedef struct dx_state_work_graphs dx_state_work_graphs;

#ifdef __cplusplus
extern "C" {
#endif

dx_shared_state* dx_shared_state_create(void);
void dx_shared_state_destroy(dx_shared_state* state);

dx_state_simple_naive* dx_state_simple_naive_create(dx_shared_state* shared_state);
void dx_state_simple_naive_destroy(dx_state_simple_naive* state);

dx_collision_compact* dx_run_simple_naive(dx_shared_state* shared_state,
										  dx_state_simple_naive* state,
										  const dx_grid_config* config,
										  const dx_entity* rigid_entities, uint32_t rigid_count,
										  const dx_entity* static_entities, uint32_t static_count,
										  const dx_shape* shapes, uint32_t shape_count,
										  bool statics_changed, uint32_t* out_count);

dx_state_simple_binned* dx_state_simple_binned_create(dx_shared_state* shared_state);
void dx_state_simple_binned_destroy(dx_state_simple_binned* state);

dx_collision_compact* dx_run_simple_binned(dx_shared_state* shared_state,
										   dx_state_simple_binned* state,
										   const dx_grid_config* config,
										   const dx_entity* rigid_entities, uint32_t rigid_count,
										   const dx_entity* static_entities, uint32_t static_count,
										   const dx_shape* shapes, uint32_t shape_count,
										   bool statics_changed, uint32_t* out_count);

dx_state_execute_indirect* dx_state_execute_indirect_create(dx_shared_state* shared_state);
void dx_state_execute_indirect_destroy(dx_state_execute_indirect* state);

dx_collision_compact* dx_run_execute_indirect(dx_shared_state* shared_state,
											  dx_state_execute_indirect* state,
											  const dx_grid_config* config,
											  const dx_entity* rigid_entities, uint32_t rigid_count,
											  const dx_entity* static_entities, uint32_t static_count,
											  const dx_shape* shapes, uint32_t shape_count,
											  bool statics_changed, uint32_t* out_count);

dx_state_work_graphs* dx_state_work_graphs_create(dx_shared_state* shared_state);
void dx_state_work_graphs_destroy(dx_state_work_graphs* state);

dx_collision_compact* dx_run_work_graphs(dx_shared_state* shared_state,
										 dx_state_work_graphs* state,
										 const dx_grid_config* config,
										 const dx_entity* rigid_entities, uint32_t rigid_count,
										 const dx_entity* static_entities, uint32_t static_count,
										 const dx_shape* shapes, uint32_t shape_count,
										 bool statics_changed, uint32_t* out_count);

#ifdef __cplusplus
}
#endif

#endif
