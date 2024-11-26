#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "renderer.h"
#include "atlas.h"

#define BUFFER_SIZE 16384

typedef uint8_t byte;

typedef struct {
    int atlas_src_id;   // source texture rect in atlas. opacity.
    mu_Rect dst_rect;   // destination on "screen"
    mu_Color dst_color; // destination color
} r_command;

static r_command cmd_buf[BUFFER_SIZE] = {};
static int buf_idx;

typedef struct {
  r_renderbuffer renderbuffer;
  mu_Rect clip_rect;
} r_framebuffer;

static r_framebuffer _framebuffer = {};

#define r_pixel(f, x, y) ((f)->renderbuffer.data[((y) * (f)->renderbuffer.width) + (x)])

void r_init(r_renderbuffer rb) {
  // init framebuffer
  memcpy(&_framebuffer.renderbuffer, &rb, sizeof(rb));
  _framebuffer.clip_rect = mu_rect(0, 0, rb.width, rb.height);

  r_clear(mu_color(0, 0, 0, 255));
}

static inline bool within(int c, int lo, int hi) {
  return c >= lo && c < hi;
}

static inline bool within_rect(mu_Rect rect, int x, int y) {
  return (within(x, rect.x, rect.x + rect.w)
          && within(y, rect.y, rect.y + rect.h));
}

static inline bool same_size(const mu_Rect* a, const mu_Rect* b) {
  return a->w == b->w && a->h == b->h;
}

static inline byte texture_color(const mu_Rect* tex, int x, int y) {
  assert(x < tex->w);
  x = x + tex->x;
  assert(y < tex->h);
  y = y + tex->y;
  return atlas_texture[ y * ATLAS_WIDTH + x];
}

static inline uint32_t r_color(mu_Color clr) {
  return ((uint32_t)clr.a << 24) | ((uint32_t)clr.r << 16) | ((uint32_t)clr.g << 8) | clr.b;
}

mu_Color mu_color_argb(uint32_t clr) {
  return mu_color((clr >> 16) & 0xff, (clr >> 8) & 0xff, clr & 0xff, (clr >> 24) & 0xff);
}

static inline int greyscale(byte c) {
  return r_color(mu_color(c, c, c, 255));
}

static inline mu_Color blend_pixel(mu_Color dst, mu_Color src) {
  int ia = 0xff - src.a;
  dst.r = ((src.r * src.a) + (dst.r * ia)) >> 8;
  dst.g = ((src.g * src.a) + (dst.g * ia)) >> 8;
  dst.b = ((src.b * src.a) + (dst.b * ia)) >> 8;
  return dst;
}

static inline mu_Color multiply_pixel(mu_Color a, mu_Color b) {
    mu_Color final =  {
        .r = (a.r * b.r) >> 8,
        .g = (a.g * b.g) >> 8,
        .b = (a.b * b.b) >> 8,
        .a = (a.a * b.a) >> 8,
    };
    return final;
}

static void flush(void) {
    // draw things based on texture, vertex, color
    for (int i = 0; i < buf_idx; i++) {
        r_command cmd = cmd_buf[i];
        mu_Rect tex = atlas[cmd.atlas_src_id];
        mu_Rect dst = cmd.dst_rect;
        mu_Color dst_color = cmd.dst_color;

        // draw
        int ystart = mu_max(dst.y, _framebuffer.clip_rect.y);
        int yend = mu_min(dst.y + dst.h, _framebuffer.clip_rect.y + _framebuffer.clip_rect.h);
        int xstart = mu_max(dst.x, _framebuffer.clip_rect.x);
        int xend = mu_min(dst.x + dst.w, _framebuffer.clip_rect.x + _framebuffer.clip_rect.w);

        mu_Real u_ratio = (mu_Real) tex.w / dst.w;
        mu_Real v_ratio = (mu_Real) tex.h / dst.h;
        for (int y = ystart; y < yend; y++) {
            for (int x = xstart; x < xend; x++) {
                assert(within_rect(dst, x, y));
                assert(within_rect(_framebuffer.clip_rect, x, y));

                mu_Color existing_color = mu_color_argb(r_pixel(&_framebuffer, x, y));
                mu_Color out_color = dst_color;

                if (cmd.atlas_src_id != ATLAS_WHITE) {
                    mu_Real u = (x - dst.x) * u_ratio;
                    mu_Real v = (y - dst.y) * v_ratio;

                    // texture contains opacity values only.
                    byte tc = texture_color(&tex, u, v);
                    out_color = multiply_pixel(mu_color(255, 255, 255, tc), out_color);
                }

                mu_Color result = blend_pixel(existing_color, out_color);
                r_pixel(&_framebuffer, x, y) = r_color(result);
            }
        }
    }
    buf_idx = 0;
}

static void push_quad(mu_Rect dst, int src_id, mu_Color color) {
    if (buf_idx == BUFFER_SIZE) { flush(); }

    cmd_buf[buf_idx] = (r_command){
        .dst_rect = dst,
        .dst_color = color,
        .atlas_src_id = src_id,
    };
    buf_idx++;
}

void r_draw_rect(mu_Rect rect, mu_Color color) {
  push_quad(rect, ATLAS_WHITE, color);
}

void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color) {
  mu_Rect dst = { pos.x, pos.y, 0, 0 };
  for (const char *p = text; *p; p++) {
    if ((*p & 0xc0) == 0x80) { continue; }
    int chr = mu_min((unsigned char) *p, 127);
    mu_Rect src = atlas[ATLAS_FONT + chr];
    dst.w = src.w;
    dst.h = src.h;
    push_quad(dst, ATLAS_FONT + chr, color);
    dst.x += dst.w;
  }
}

void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
  mu_Rect src = atlas[id];
  int x = rect.x + (rect.w - src.w) / 2;
  int y = rect.y + (rect.h - src.h) / 2;
  push_quad(mu_rect(x, y, src.w, src.h), id, color);
}

int r_get_text_width(const char *text, int len) {
  int res = 0;
  for (const char *p = text; *p && len--; p++) {
    if ((*p & 0xc0) == 0x80) { continue; }
    int chr = mu_min((unsigned char) *p, 127);
    res += atlas[ATLAS_FONT + chr].w;
  }
  return res;
}

int r_get_text_height(void) {
  return 18;
}

void r_set_clip_rect(mu_Rect rect) {
  flush();
  // TODO: we could simply store a clip stack to avoid flushes...
  int ystart = mu_max(0, rect.y);
  int yend = mu_min(_framebuffer.renderbuffer.height, rect.y + rect.h);
  int xstart = mu_max(0, rect.x);
  int xend = mu_min(_framebuffer.renderbuffer.width, rect.x + rect.w);
  _framebuffer.clip_rect = mu_rect(xstart, ystart, xend - xstart, yend - ystart);
}

void r_clear(mu_Color clr) {
    flush(); // TODO: we don't need to flush if everything will be discarded. need to reset buffidx tho.
    for (int i = 0; i < _framebuffer.renderbuffer.width * _framebuffer.renderbuffer.height; i++) {
        _framebuffer.renderbuffer.data[i] = r_color(clr);
    }
}

void r_present(void) {
  flush();
}

void r_line(int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = (dx > dy ? dx : -dy) / 2, e2;
    for (;;) {
        if (within_rect(_framebuffer.clip_rect, x0, y0)) {
            r_pixel(&_framebuffer, x0, y0) = c;
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy) {
            err += dx;
            y0 += sy;
        }
    }
}
