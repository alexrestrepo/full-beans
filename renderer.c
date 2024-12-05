#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <tgmath.h>

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
//    union {
//        uint32_t value;
//        struct {
//            uint8_t r;
//            uint8_t g;
//            uint8_t b;
//            uint8_t a;
//        } col;
//    }
  return ((uint32_t)clr.a << 24) | ((uint32_t)clr.r << 16) | ((uint32_t)clr.g << 8) | clr.b;
}

static inline mu_Color mu_color_argb(uint32_t clr) {
    return (mu_Color) {
        .a = (clr >> 24) & 0xff,
        .r = (clr >> 16) & 0xff,
        .g = (clr >> 8)  & 0xff,
        .b = clr & 0xff,
    };
//  return mu_color((clr >> 16) & 0xff, (clr >> 8) & 0xff, clr & 0xff, (clr >> 24) & 0xff);
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

                mu_Color result = out_color.a < 255 ? blend_pixel(existing_color, out_color) : out_color;
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

void r_wu_line(int x0, int y0, int x1, int y1, uint32_t c) {
    mu_Color line_color = mu_color_argb(c);

#define r_swap(x, y) { int tmp = x; x = y; y = tmp; }
    if (abs(y1 - y0) < abs(x1 - x0)) {
        if (x1 < x0) {
            r_swap(x0, x1);
            r_swap(y0, y1);
        }

        float dx = x1 - x0;
        float dy = y1 - y0;
        float m = dy / dx;

        for (int i = 0; i < dx; i++) {
            float y = y0 + i * m;
            int ix = x0 + i;
            int iy = (int)y;

            if (within_rect(_framebuffer.clip_rect, ix, iy)) {
                float dist = fabs(y - iy);

                mu_Color existing_color = mu_color_argb(r_pixel(&_framebuffer, ix, iy));
                line_color.a = 255.0 * (1.0 - dist);
                mu_Color result = line_color.a < 255 ? blend_pixel(existing_color, line_color) : line_color;
                r_pixel(&_framebuffer, ix, iy) = r_color(result);

                existing_color = mu_color_argb(r_pixel(&_framebuffer, ix, iy + 1));
                line_color.a = 255.0 * (dist);
                result = line_color.a < 255 ? blend_pixel(existing_color, line_color) : line_color;
                r_pixel(&_framebuffer, ix, iy + 1) = r_color(result);
            }
        }

    } else {
        if (y1 < y0) {
            r_swap(x0, x1);
            r_swap(y0, y1);
        }

        float dx = x1 - x0;
        float dy = y1 - y0;
        float m = dx / dy;

        for (int i = 0; i < dy; i++) {
            float x = x0 + i * m;
            int ix = (int)x;
            int iy = y0 + i;

            if (within_rect(_framebuffer.clip_rect, ix, iy)) {
                float dist = fabs(x - ix);

                mu_Color existing_color = mu_color_argb(r_pixel(&_framebuffer, ix, iy));
                line_color.a = 255.0 * (1.0 - dist);
                mu_Color result = line_color.a < 255 ? blend_pixel(existing_color, line_color) : line_color;
                r_pixel(&_framebuffer, ix, iy) = r_color(result);

                existing_color = mu_color_argb(r_pixel(&_framebuffer, ix + 1, iy));
                line_color.a = 255.0 * (dist);
                result = line_color.a < 255 ? blend_pixel(existing_color, line_color) : line_color;
                r_pixel(&_framebuffer, ix + 1, iy) = r_color(result);
            }
        }
    }
#undef r_swap
}

// https://jtsorlinis.github.io/rendering-tutorial/
static inline float edge_function(mu_Vec2 a, mu_Vec2 b, mu_Vec2 c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

void r_triangle(mu_Vec2 a, mu_Color ca, mu_Vec2 b, mu_Color cb, mu_Vec2 c, mu_Color cc) {
    // Calculate the edge function for the whole triangle (ABC)
    float ABC = edge_function(a, b, c);

    // Our nifty trick: Don't bother drawing the triangle if it's back facing
    if (ABC < 0) {
        return;
    }

    mu_Vec2 p = {0};

    // Get the bounding box of the triangle
    int minX = mu_min(a.x, mu_min(b.x, c.x));
    int minY = mu_min(a.y, mu_min(b.y, c.y));
    int maxX = mu_max(a.x, mu_max(b.x, c.x));
    int maxY = mu_max(a.y, mu_max(b.y, c.y));

    // Loop through all the pixels of the bounding box
    for (p.y = minY; p.y < maxY; p.y++) {
        for (p.x = minX; p.x < maxX; p.x++) {
            if (within_rect(_framebuffer.clip_rect, p.x, p.y)) {

                // Calculate our edge functions
                float ABP = edge_function(a, b, p);
                float BCP = edge_function(b, c, p);
                float CAP = edge_function(c, a, p);

                // If all the edge functions are positive, the point is inside the triangle
                if (ABP >= 0 && BCP >= 0 && CAP >= 0) {
                    // Normalise the edge functions by dividing by the total area to get the barycentric coordinates
                    float weightA = BCP / ABC;
                    float weightB = CAP / ABC;
                    float weightC = ABP / ABC;

                    // Interpolate the colours at point P
                    int r = ca.r * weightA + cb.r * weightB + cc.r * weightC;
                    int g = ca.g * weightA + cb.g * weightB + cc.g * weightC;
                    int b = ca.b * weightA + cb.b * weightB + cc.b * weightC;
                    int a = ca.a * weightA + cb.a * weightB + cc.a * weightC;
                    mu_Color cp = mu_color(r, g, b, a);

                    // Draw the pixel
                    mu_Color existing_color = mu_color_argb(r_pixel(&_framebuffer, p.x, p.y));
                    mu_Color result = a < 255 ? blend_pixel(existing_color, cp) : cp;
                    r_pixel(&_framebuffer, p.x, p.y) = r_color(result);
                }
            }
        }
    }
}

// https://www.computerenhance.com/p/efficient-dda-circle-outlines
void r_circle(mu_Vec2 center, int radius, mu_Color color) {
    // NOTE(casey): Center and radius of the circle
    int Cx = center.x;
    int Cy = center.y;
    int R = radius;

    // NOTE(casey): Loop that draws the circle
    {
        int R2 = R + R;

        int X = R;
        int Y = 0;
        int dY = -2;
        int dX = R2 + R2 - 4;
        int D = R2 - 1;

        while(Y <= X) {
            if (within_rect(_framebuffer.clip_rect, Cx - X, Cy - Y)) { r_pixel(&_framebuffer, Cx - X, Cy - Y) = r_color(color); }
            if (within_rect(_framebuffer.clip_rect, Cx + X, Cy - Y)) { r_pixel(&_framebuffer, Cx + X, Cy - Y) = r_color(color); }
            if (within_rect(_framebuffer.clip_rect, Cx - X, Cy + Y)) { r_pixel(&_framebuffer, Cx - X, Cy + Y) = r_color(color); }
            if (within_rect(_framebuffer.clip_rect, Cx + X, Cy + Y)) { r_pixel(&_framebuffer, Cx + X, Cy + Y) = r_color(color); }
            if (within_rect(_framebuffer.clip_rect, Cx - Y, Cy - X)) { r_pixel(&_framebuffer, Cx - Y, Cy - X) = r_color(color); }
            if (within_rect(_framebuffer.clip_rect, Cx + Y, Cy - X)) { r_pixel(&_framebuffer, Cx + Y, Cy - X) = r_color(color); }
            if (within_rect(_framebuffer.clip_rect, Cx - Y, Cy + X)) { r_pixel(&_framebuffer, Cx - Y, Cy + X) = r_color(color); }
            if (within_rect(_framebuffer.clip_rect, Cx + Y, Cy + X)) { r_pixel(&_framebuffer, Cx + Y, Cy + X) = r_color(color); }

            D += dY;
            dY -= 4;
            ++Y;

#if 0
            // NOTE(casey): Branching version
            if(D < 0) {
                D += dX;
                dX -= 4;
                --X;
            }
#else
            // NOTE(casey): Branchless version
            int Mask = (D >> 31);
            D += dX & Mask;
            dX -= 4 & Mask;
            X += Mask;
#endif
        }
    }
}

// filled circle
// https://web.archive.org/web/20120422045142/https://banu.com/blog/7/drawing-circles/
void r_fill_circle(mu_Vec2 center, int radius, mu_Color color) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if ((x * x) + (y * y) <= (radius * radius) && within_rect(_framebuffer.clip_rect, center.x + x, center.y + y)) {
                r_pixel(&_framebuffer, center.x + x, center.y + y) = r_color(color);                
            }
        }
    }
}

#undef r_pixel
