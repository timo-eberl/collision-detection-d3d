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

typedef struct dx_shared_state dx_shared_state;
typedef struct dx_state_collision dx_state_collision;

#ifdef __cplusplus
extern "C" {
#endif

dx_shared_state* dx_shared_state_create(void);
void dx_shared_state_destroy(dx_shared_state* state);

dx_state_collision* dx_state_collision_create(dx_shared_state* shared_state);
void dx_state_collision_destroy(dx_state_collision* state);

dx_collision_compact* dx_run_collision(dx_shared_state* shared_state, dx_state_collision* state,
									   const dx_entity* rigid_entities, uint32_t rigid_count,
									   const dx_entity* static_entities, uint32_t static_count,
									   const dx_shape* shapes, uint32_t shape_count,
									   bool statics_changed, uint32_t* out_count);

#ifdef __cplusplus
}
#endif

#endif
