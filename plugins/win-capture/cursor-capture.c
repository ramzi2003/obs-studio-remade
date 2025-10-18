#include <windows.h>
#include <obs.h>
#include <obs-module.h>
#include "cursor-capture.h"

static uint8_t *get_bitmap_data(HBITMAP hbmp, BITMAP *bmp)
{
	if (GetObject(hbmp, sizeof(*bmp), bmp) != 0) {
		uint8_t *output;
		unsigned int size = bmp->bmHeight * bmp->bmWidthBytes;
		if (!size) {
			return NULL;
		}

		output = bmalloc(size);
		GetBitmapBits(hbmp, size, output);
		return output;
	}

	return NULL;
}

static inline uint8_t bit_to_alpha(uint8_t *data, long pixel, bool invert)
{
	uint8_t pix_byte = data[pixel / 8];
	bool alpha = (pix_byte >> (7 - pixel % 8) & 1) != 0;

	if (invert) {
		return alpha ? 0xFF : 0;
	} else {
		return alpha ? 0 : 0xFF;
	}
}

static inline bool bitmap_has_alpha(uint8_t *data, long num_pixels)
{
	for (long i = 0; i < num_pixels; i++) {
		if (data[i * 4 + 3] != 0) {
			return true;
		}
	}

	return false;
}

static inline void apply_mask(uint8_t *color, uint8_t *mask, BITMAP *bmp_mask)
{
	long mask_pix_offs;

	for (long y = 0; y < bmp_mask->bmHeight; y++) {
		for (long x = 0; x < bmp_mask->bmWidth; x++) {
			mask_pix_offs = y * (bmp_mask->bmWidthBytes * 8) + x;
			color[(y * bmp_mask->bmWidth + x) * 4 + 3] = bit_to_alpha(mask, mask_pix_offs, false);
		}
	}
}

static inline uint8_t *copy_from_color(ICONINFO *ii, uint32_t *width, uint32_t *height)
{
	BITMAP bmp_color;
	BITMAP bmp_mask;
	uint8_t *color;
	uint8_t *mask;

	color = get_bitmap_data(ii->hbmColor, &bmp_color);
	if (!color) {
		return NULL;
	}

	if (bmp_color.bmBitsPixel < 32) {
		bfree(color);
		return NULL;
	}

	mask = get_bitmap_data(ii->hbmMask, &bmp_mask);
	if (mask) {
		long pixels = bmp_color.bmHeight * bmp_color.bmWidth;

		if (!bitmap_has_alpha(color, pixels))
			apply_mask(color, mask, &bmp_mask);

		bfree(mask);
	}

	*width = bmp_color.bmWidth;
	*height = bmp_color.bmHeight;
	return color;
}

static inline uint8_t *copy_from_mask(ICONINFO *ii, uint32_t *width, uint32_t *height)
{
	uint8_t *output;
	uint8_t *mask;
	long pixels;
	long bottom;
	BITMAP bmp;

	mask = get_bitmap_data(ii->hbmMask, &bmp);
	if (!mask) {
		return NULL;
	}

	bmp.bmHeight /= 2;

	pixels = bmp.bmHeight * bmp.bmWidth;
	if (!pixels) {
		return NULL;
	}
	output = bzalloc(pixels * 4);

	bottom = bmp.bmWidthBytes * bmp.bmHeight;

	for (long i = 0; i < pixels; i++) {
		uint8_t andMask = bit_to_alpha(mask, i, true);
		uint8_t xorMask = bit_to_alpha(mask + bottom, i, true);

		if (!andMask) {
			// black in the AND mask
			*(uint32_t *)&output[i * 4] = !!xorMask ? 0x00FFFFFF /*always white*/
								: 0xFF000000 /*always black*/;
		} else {
			// white in the AND mask
			*(uint32_t *)&output[i * 4] = !!xorMask ? 0xFFFFFFFF /*source inverted*/
								: 0 /*transparent*/;
		}
	}

	bfree(mask);

	*width = bmp.bmWidth;
	*height = bmp.bmHeight;
	return output;
}

static inline uint8_t *cursor_capture_icon_bitmap(ICONINFO *ii, uint32_t *width, uint32_t *height, bool *monochrome)
{
	uint8_t *output;
	*monochrome = false;
	output = copy_from_color(ii, width, height);
	if (!output) {
		*monochrome = true;
		output = copy_from_mask(ii, width, height);
	}

	return output;
}

static gs_texture_t *get_cached_texture(struct cursor_data *data, uint32_t cx, uint32_t cy)
{
	struct cached_cursor cc;

	for (size_t i = 0; i < data->cached_textures.num; i++) {
		struct cached_cursor *pcc = &data->cached_textures.array[i];

		if (pcc->cx == cx && pcc->cy == cy)
			return pcc->texture;
	}

	cc.texture = gs_texture_create(cx, cy, GS_BGRA, 1, NULL, GS_DYNAMIC);
	cc.cx = cx;
	cc.cy = cy;
	da_push_back(data->cached_textures, &cc);
	return cc.texture;
}

static inline bool cursor_capture_icon(struct cursor_data *data, HICON icon)
{
	uint8_t *bitmap;
	uint32_t height;
	uint32_t width;
	ICONINFO ii;

	if (!icon) {
		return false;
	}
	if (!GetIconInfo(icon, &ii)) {
		return false;
	}

	bitmap = cursor_capture_icon_bitmap(&ii, &width, &height, &data->monochrome);
	if (bitmap) {
		if (data->last_cx != width || data->last_cy != height) {
			data->texture = get_cached_texture(data, width, height);
			data->last_cx = width;
			data->last_cy = height;
		}
		gs_texture_set_image(data->texture, bitmap, width * 4, false);
		bfree(bitmap);

		data->x_hotspot = ii.xHotspot;
		data->y_hotspot = ii.yHotspot;
	}

	DeleteObject(ii.hbmColor);
	DeleteObject(ii.hbmMask);
	return !!data->texture;
}

void cursor_capture(struct cursor_data *data)
{
	CURSORINFO ci = {0};
	HICON icon;

	ci.cbSize = sizeof(ci);

	if (!GetCursorInfo(&ci)) {
		data->visible = false;
		return;
	}

	memcpy(&data->cursor_pos, &ci.ptScreenPos, sizeof(data->cursor_pos));

	if (data->current_cursor == ci.hCursor) {
		return;
	}

	icon = CopyIcon(ci.hCursor);
	data->visible = cursor_capture_icon(data, icon);
	data->current_cursor = ci.hCursor;
	if ((ci.flags & CURSOR_SHOWING) == 0)
		data->visible = false;
	DestroyIcon(icon);
}

void cursor_draw(struct cursor_data *data, long x_offset, long y_offset, long width, long height, bool force_png)
{
	long x = data->cursor_pos.x + x_offset;
	long y = data->cursor_pos.y + y_offset;
	
	// Check if cursor is inside the window
	bool inside_window = (x >= 0 && x <= width && y >= 0 && y <= height);
	
	gs_texture_t *cursor_texture;
	uint32_t cursor_cx, cursor_cy;
	long hotspot_x, hotspot_y;
	
	// Update last valid position if cursor is inside
	if (inside_window) {
		data->last_valid_pos.x = x;
		data->last_valid_pos.y = y;
		data->has_valid_pos = true;
	}
	
	// Use PNG cursor if forced (window not focused) or cursor is outside window
	if (force_png || !inside_window) {
		// Use frozen position if outside window or no valid position yet
		if (!inside_window && data->has_valid_pos) {
			x = data->last_valid_pos.x;
			y = data->last_valid_pos.y;
			
			// Clamp to window boundaries
			if (x < 0) x = 0;
			if (x > width) x = width;
			if (y < 0) y = 0;
			if (y > height) y = height;
		} else if (!inside_window) {
			// Never entered the window, don't draw cursor
			blog(LOG_INFO, "[cursor_draw] No valid position - not drawing");
			return;
		}
		
		// Use PNG cursor if available, otherwise fallback to system cursor
		if (data->custom_cursor_texture) {
			cursor_texture = data->custom_cursor_texture;
			cursor_cx = data->custom_cursor_cx;
			cursor_cy = data->custom_cursor_cy;
			hotspot_x = cursor_cx / 2;  // Center hotspot for PNG
			hotspot_y = cursor_cy / 2;
			blog(LOG_INFO, "[cursor_draw] Using PNG: force=%d inside=%d pos=(%ld,%ld)", force_png, inside_window, x, y);
		} else {
			cursor_texture = data->texture;
			cursor_cx = data->last_cx;
			cursor_cy = data->last_cy;
			hotspot_x = data->x_hotspot;
			hotspot_y = data->y_hotspot;
			blog(LOG_INFO, "[cursor_draw] Using system cursor fallback: pos=(%ld,%ld)", x, y);
		}
		
	} else {
		// Window is focused and cursor is inside - use real system cursor
		cursor_texture = data->texture;
		cursor_cx = data->last_cx;
		cursor_cy = data->last_cy;
		hotspot_x = data->x_hotspot;
		hotspot_y = data->y_hotspot;
		
		blog(LOG_INFO, "[cursor_draw] Inside window focused: pos=(%ld,%ld) texture=%p", x, y, cursor_texture);
	}
	
	long x_draw = x - hotspot_x;
	long y_draw = y - hotspot_y;

	blog(LOG_INFO, "[cursor_draw] visible=%d cursor_texture=%p draw_pos=(%ld,%ld)", 
	     data->visible, cursor_texture, x_draw, y_draw);

	if (data->visible && cursor_texture) {
		gs_effect_t *effect = obs_get_base_effect(OBS_EFFECT_DEFAULT);
		
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

		gs_matrix_push();
		gs_matrix_translate3f((float)x_draw, (float)y_draw, 0.0f);
		
		while (gs_effect_loop(effect, "Draw")) {
			gs_effect_set_texture(gs_effect_get_param_by_name(effect, "image"), cursor_texture);
			gs_draw_sprite(cursor_texture, 0, 0, 0);
		}
		
		gs_matrix_pop();

		gs_blend_state_pop();
		blog(LOG_INFO, "[cursor_draw] Drew cursor successfully with effect");
	} else {
		blog(LOG_INFO, "[cursor_draw] Skipped drawing: visible=%d texture=%p", data->visible, cursor_texture);
	}
}

void cursor_data_init(struct cursor_data *data)
{
	// Load custom cursor PNG from data folder
	char *cursor_file = obs_module_file("cursor.png");
	blog(LOG_INFO, "[cursor_data_init] cursor_file path: %s", cursor_file ? cursor_file : "NULL");
	
	if (cursor_file) {
		gs_image_file4_t image = {0};
		gs_image_file4_init(&image, cursor_file, GS_IMAGE_ALPHA_PREMULTIPLY);
		
		obs_enter_graphics();
		gs_image_file4_init_texture(&image);
		
		struct gs_image_file *img = &image.image3.image2.image;
		if (img->texture) {
			data->custom_cursor_texture = img->texture;
			data->custom_cursor_cx = img->cx;
			data->custom_cursor_cy = img->cy;
			blog(LOG_INFO, "[cursor_data_init] PNG cursor loaded successfully: %dx%d", img->cx, img->cy);
			// Don't free the texture, we'll keep using it
			img->texture = NULL;
		} else {
			blog(LOG_WARNING, "[cursor_data_init] Failed to load PNG cursor texture");
		}
		
		obs_leave_graphics();
		gs_image_file4_free(&image);
		bfree(cursor_file);
	} else {
		blog(LOG_WARNING, "[cursor_data_init] cursor.png file not found");
	}
}

void cursor_data_free(struct cursor_data *data)
{
	for (size_t i = 0; i < data->cached_textures.num; i++) {
		struct cached_cursor *pcc = &data->cached_textures.array[i];
		gs_texture_destroy(pcc->texture);
	}
	da_free(data->cached_textures);
	
	obs_enter_graphics();
	if (data->custom_cursor_texture) {
		gs_texture_destroy(data->custom_cursor_texture);
	}
	obs_leave_graphics();
	
	memset(data, 0, sizeof(*data));
}
