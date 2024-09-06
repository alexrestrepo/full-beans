#include "fenster.h"
#include<stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "renderer.h"
#include "atlas.inl"

#define BUFFER_SIZE 16384

typedef uint8_t byte;

static mu_Rect tex_buf[BUFFER_SIZE];
static mu_Rect src_buf[BUFFER_SIZE];
static byte      color_buf[BUFFER_SIZE * 4];

static int buf_idx;

static struct fenster window = (struct fenster){.title="A window", .width=800, .height=600};

static mu_Rect clip_rect;

void *r_window(void) {
  return &window;
}

void r_init(void) {
  /* init SDL window */
  window.buf = malloc(window.width * window.height * sizeof(*window.buf));
  r_clear(mu_color(0, 0, 0, 255));
  fenster_open(&window);

  /* init texture */
}

static inline bool within(int c, int lo, int hi) {
  return c >= lo && c < hi;
}

static inline bool within_rect(mu_Rect rect, int x, int y) {
  return within(x, rect.x, rect.x+rect.w)
        && within(y, rect.y, rect.y+rect.h);
}

static inline bool same_size(const mu_Rect* a, const mu_Rect* b) {
  return a->w == b->w && a->h == b->h;
}

static inline byte texture_color(const mu_Rect* tex, int x, int y) {
  assert(x < tex->w);
  x = x + tex->x;
  assert(y < tex->h);
  y = y + tex->y;
  return atlas_texture[y*ATLAS_WIDTH + x];
}

static inline int color(byte r, byte g, byte b) {  // ignore alpha channel for now
  int result = r;
  result = result << 8;
  result = result & g;
  result = result << 8;
  result = result & b;
  return result;
}

static inline int greyscale(byte c) {
  return color(c, c, c);
}


static void flush(void) {
  if (buf_idx == 0) { return; }

  // draw things based on texture, vertex, color
  for (int i = 0; i < BUFFER_SIZE; i++) {
    mu_Rect* src = &src_buf[i];
    mu_Rect* tex = &tex_buf[i];
    int c = color(color_buf[i], color_buf[i+1], color_buf[i+2]);
    // draw
    for (int y = src->y; y < src->y+src->h; y++) {
      for (int x = src->x; x < src->x+src->w; x++) {
        if (within_rect(clip_rect, x, y)) {
          // hacky but sufficient for us
          if (same_size(src, tex)) {
            // read color from texture
            fenster_pixel(&window, x, y) = greyscale(texture_color(tex, x-src->x, y-src->y));
          }
          else {
            // use color from operation
//?             fenster_pixel(&window, x, y) = c;
          }
        }
      }
    }
  }

  buf_idx = 0;
}


static void push_quad(mu_Rect src, mu_Rect tex, mu_Color color) {
  if (buf_idx == BUFFER_SIZE) { flush(); }

  int color_idx = buf_idx *  4;

  memcpy(&tex_buf[buf_idx], &tex, sizeof(mu_Rect));
  memcpy(&src_buf[buf_idx], &src, sizeof(mu_Rect));
  memcpy(color_buf + color_idx, &color, 4);

  buf_idx++;
}


void r_draw_rect(mu_Rect rect, mu_Color color) {
  push_quad(rect, atlas[ATLAS_WHITE], color);
}


void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color) {
  mu_Rect dst = { pos.x, pos.y, 0, 0 };
  for (const char *p = text; *p; p++) {
    if ((*p & 0xc0) == 0x80) { continue; }
    int chr = mu_min((unsigned char) *p, 127);
    mu_Rect src = atlas[ATLAS_FONT + chr];
    dst.w = src.w;
    dst.h = src.h;
    push_quad(dst, src, color);
    dst.x += dst.w;
  }
}


void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
  mu_Rect src = atlas[id];
  int x = rect.x + (rect.w - src.w) / 2;
  int y = rect.y + (rect.h - src.h) / 2;
  push_quad(mu_rect(x, y, src.w, src.h), src, color);
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
  memcpy(&clip_rect, &rect, sizeof(mu_Rect));
}

uint32_t r_color(mu_Color clr) {
  return ((uint32_t)clr.a << 24) | ((uint32_t)clr.b << 16) | ((uint32_t)clr.g << 8) | clr.r;
}

void r_clear(mu_Color clr) {
  flush();
  for (int i = 0; i < window.width * window.height; i++) {
    window.buf[i] = r_color(clr);
  }
}


void r_present(void) {
  flush();
  fenster_loop(&window);
}
