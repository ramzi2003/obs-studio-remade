#pragma once

#include <stdint.h>
#include <graphics/image-file.h>

struct cached_cursor {
	gs_texture_t *texture;
	uint32_t cx;
	uint32_t cy;
};

struct cursor_data {
	gs_texture_t *texture;
	gs_texture_t *custom_cursor_texture;  // Custom cursor PNG texture
	HCURSOR current_cursor;
	POINT cursor_pos;
	POINT last_valid_pos;  // Store last valid position inside window
	bool has_valid_pos;    // Track if we ever had a valid position
	long x_hotspot;
	long y_hotspot;
	bool visible;
	bool monochrome;

	uint32_t last_cx;
	uint32_t last_cy;
	uint32_t custom_cursor_cx;  // Custom cursor width
	uint32_t custom_cursor_cy;  // Custom cursor height

	DARRAY(struct cached_cursor) cached_textures;
};

extern void cursor_capture(struct cursor_data *data);
extern void cursor_draw(struct cursor_data *data, long x_offset, long y_offset, long width, long height, bool force_png);
extern void cursor_data_free(struct cursor_data *data);
extern void cursor_data_init(struct cursor_data *data);
