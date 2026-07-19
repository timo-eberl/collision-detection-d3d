#ifndef COLLISION_DETECTION_D3D
#define COLLISION_DETECTION_D3D

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
	float p_a[3];
	float radius;
	float p_b[3];
	uint32_t type;
} dx_shape;

typedef struct {
	uint32_t a_index;
	uint32_t b_index;
	uint32_t b_type;
	float depth;
	float point_a[3];
	float point_b[3];
	float normal[3];
	uint32_t pad[3];
} dx_collision;

typedef struct dx_shared_state dx_shared_state;
typedef struct dx_state_collision dx_state_collision;

#ifdef __cplusplus
extern "C" {
#endif

dx_shared_state* dx_shared_state_create(void);
void dx_shared_state_destroy(dx_shared_state* state);

dx_state_collision* dx_state_collision_create(dx_shared_state* shared_state);
void dx_state_collision_destroy(dx_state_collision* state);

dx_collision* dx_run_collision(dx_shared_state* shared_state, dx_state_collision* state,
                               const dx_shape* rigids, uint32_t rigid_count,
                               const dx_shape* statics, uint32_t static_count,
                               bool statics_changed, uint32_t* out_count);

#ifdef __cplusplus
}
#endif

#endif
