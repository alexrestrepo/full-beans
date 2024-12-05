#ifndef RENDERER_H
#define RENDERER_H

#include "microui.h"

#include <stdint.h>

typedef struct {
    uint32_t *data;
    const int width;
    const int height;
} r_renderbuffer;

void r_init(r_renderbuffer renderbuffer);
void r_draw_rect(mu_Rect rect, mu_Color color);
void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color);
void r_draw_icon(int id, mu_Rect rect, mu_Color color);
 int r_get_text_width(const char *text, int len);
 int r_get_text_height(void);
void r_set_clip_rect(mu_Rect rect);
void r_clear(mu_Color color);
void r_present(void);

void r_line(int x0, int y0, int x1, int y1, uint32_t c);
void r_wu_line(int x0, int y0, int x1, int y1, uint32_t c);
void r_triangle(mu_Vec2 a, mu_Color ca, mu_Vec2 b, mu_Color cb, mu_Vec2 c, mu_Color cc);
void r_circle(mu_Vec2 center, int radius, mu_Color color);
void r_fill_circle(mu_Vec2 center, int radius, mu_Color color);
#endif

