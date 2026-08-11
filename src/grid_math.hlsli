#ifndef GRID_MATH_HLSLI
#define GRID_MATH_HLSLI

int clamp_int(int v, int lo, int hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

int cell_coord(float pos, float origin, float cell_size, int grid_res) {
	return clamp_int((int)floor((pos - origin) / cell_size), 0, grid_res - 1);
}

uint cell_index(int cx, int cy, int cz, int res_x, int res_y) {
	return (uint)cx + (uint)cy * res_x + (uint)cz * res_x * res_y;
}

#endif
