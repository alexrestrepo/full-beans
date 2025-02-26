#include "renderer.h"
#include "microui.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>

#include "fenster.h"

static  char logbuf[64000];
static   int logbuf_updated = 0;
static float bg[3] = { 90, 95, 100 };


static void write_log(const char *text) {
    if (logbuf[0]) { strcat(logbuf, "\n"); }
    strcat(logbuf, text);
    logbuf_updated = 1;
}


static void test_window(mu_Context *ctx) {
    /* do window */
    if (mu_begin_window(ctx, "Demo Window", mu_rect(40, 40, 300, 450))) {
        mu_Container *win = mu_get_current_container(ctx);
        win->rect.w = mu_max(win->rect.w, 240);
        win->rect.h = mu_max(win->rect.h, 300);

        /* window info */
        if (mu_header(ctx, "Window Info")) {
            mu_Container *win = mu_get_current_container(ctx);
            char buf[64];
            mu_layout_row(ctx, 2, (int[]) { 54, -1 }, 0);
            mu_label(ctx,"Position:");
            sprintf(buf, "%d, %d", win->rect.x, win->rect.y); mu_label(ctx, buf);
            mu_label(ctx, "Size:");
            sprintf(buf, "%d, %d", win->rect.w, win->rect.h); mu_label(ctx, buf);
        }

        /* labels + buttons */
        if (mu_header_ex(ctx, "Test Buttons", MU_OPT_EXPANDED)) {
            mu_layout_row(ctx, 3, (int[]) { 86, -110, -1 }, 0);
            mu_label(ctx, "Test buttons 1:");
            if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
            if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
            mu_label(ctx, "Test buttons 2:");
            if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
            if (mu_button(ctx, "Popup")) { mu_open_popup(ctx, "Test Popup"); }
            if (mu_begin_popup(ctx, "Test Popup")) {
                if (mu_button(ctx, "Hello")) { write_log("pressed Hello"); };
                if (mu_button(ctx, "World")) { write_log("pressed World"); };
                if (mu_button(ctx, "Full")) { write_log("pressed Full"); };
                if (mu_button(ctx, "Beans!")) { write_log("pressed Beans!"); };
                mu_end_popup(ctx);
            }
        }

        /* tree */
        if (mu_header_ex(ctx, "Tree and Text", MU_OPT_EXPANDED)) {
            mu_layout_row(ctx, 2, (int[]) { 140, -1 }, 0);
            mu_layout_begin_column(ctx);
            if (mu_begin_treenode(ctx, "Test 1")) {
                if (mu_begin_treenode(ctx, "Test 1a")) {
                    mu_label(ctx, "Hello");
                    mu_label(ctx, "world");
                    mu_end_treenode(ctx);
                }
                if (mu_begin_treenode(ctx, "Test 1b")) {
                    if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
                    if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
                    mu_end_treenode(ctx);
                }
                mu_end_treenode(ctx);
            }
            if (mu_begin_treenode(ctx, "Test 2")) {
                mu_layout_row(ctx, 2, (int[]) { 54, 54 }, 0);
                if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
                if (mu_button(ctx, "Button 4")) { write_log("Pressed button 4"); }
                if (mu_button(ctx, "Button 5")) { write_log("Pressed button 5"); }
                if (mu_button(ctx, "Button 6")) { write_log("Pressed button 6"); }
                mu_end_treenode(ctx);
            }
            if (mu_begin_treenode(ctx, "Test 3")) {
                static int checks[3] = { 1, 0, 1 };
                mu_checkbox(ctx, "Checkbox 1", &checks[0]);
                mu_checkbox(ctx, "Checkbox 2", &checks[1]);
                mu_checkbox(ctx, "Checkbox 3", &checks[2]);
                mu_end_treenode(ctx);
            }
            mu_layout_end_column(ctx);

            mu_layout_begin_column(ctx);
            mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
            mu_text(ctx, "Lorem ipsum dolor sit amet, consectetur adipiscing "
                    "elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
                    "ipsum, eu varius magna felis a nulla.");
            mu_layout_end_column(ctx);
        }

        /* background color sliders */
        if (mu_header_ex(ctx, "Background Color", MU_OPT_EXPANDED)) {
            mu_layout_row(ctx, 2, (int[]) { -78, -1 }, 74);
            /* sliders */
            mu_layout_begin_column(ctx);
            mu_layout_row(ctx, 2, (int[]) { 46, -1 }, 0);
            mu_label(ctx, "Red:");   mu_slider(ctx, &bg[0], 0, 255);
            mu_label(ctx, "Green:"); mu_slider(ctx, &bg[1], 0, 255);
            mu_label(ctx, "Blue:");  mu_slider(ctx, &bg[2], 0, 255);
            mu_layout_end_column(ctx);
            /* color preview */
            mu_Rect r = mu_layout_next(ctx);
            mu_draw_rect(ctx, r, mu_color(bg[0], bg[1], bg[2], 255));
            char buf[32];
            sprintf(buf, "#%02X%02X%02X", (int) bg[0], (int) bg[1], (int) bg[2]);
            mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
        }

        mu_end_window(ctx);
    }
}


static void log_window(mu_Context *ctx) {
    if (mu_begin_window(ctx, "Log Window", mu_rect(350, 40, 300, 200))) {
        /* output text panel */
        mu_layout_row(ctx, 1, (int[]) { -1 }, -25);
        mu_begin_panel(ctx, "Log Output");
        mu_Container *panel = mu_get_current_container(ctx);
        mu_layout_row(ctx, 1, (int[]) { -1 }, -1);
        mu_text(ctx, logbuf);
        mu_end_panel(ctx);
        if (logbuf_updated) {
            panel->scroll.y = panel->content_size.y;
            logbuf_updated = 0;
        }

        /* input textbox + submit button */
        static char buf[128];
        int submitted = 0;
        mu_layout_row(ctx, 2, (int[]) { -70, -1 }, 0);
        if (mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) {
            mu_set_focus(ctx, ctx->last_id);
            submitted = 1;
        }
        if (mu_button(ctx, "Submit")) { submitted = 1; }
        if (submitted) {
            write_log(buf);
            buf[0] = '\0';
        }

        mu_end_window(ctx);
    }
}


static int uint8_slider(mu_Context *ctx, unsigned char *value, int low, int high) {
    static float tmp;
    mu_push_id(ctx, &value, sizeof(value));
    tmp = *value;
    int res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
    *value = tmp;
    mu_pop_id(ctx);
    return res;
}


static void style_window(mu_Context *ctx) {
    static struct { const char *label; int idx; } colors[] = {
        { "text:",         MU_COLOR_TEXT        },
        { "border:",       MU_COLOR_BORDER      },
        { "windowbg:",     MU_COLOR_WINDOWBG    },
        { "titlebg:",      MU_COLOR_TITLEBG     },
        { "titletext:",    MU_COLOR_TITLETEXT   },
        { "panelbg:",      MU_COLOR_PANELBG     },
        { "button:",       MU_COLOR_BUTTON      },
        { "buttonhover:",  MU_COLOR_BUTTONHOVER },
        { "buttonfocus:",  MU_COLOR_BUTTONFOCUS },
        { "base:",         MU_COLOR_BASE        },
        { "basehover:",    MU_COLOR_BASEHOVER   },
        { "basefocus:",    MU_COLOR_BASEFOCUS   },
        { "scrollbase:",   MU_COLOR_SCROLLBASE  },
        { "scrollthumb:",  MU_COLOR_SCROLLTHUMB },
        { NULL }
    };

    if (mu_begin_window(ctx, "Style Editor", mu_rect(350, 250, 300, 240))) {
        int sw = mu_get_current_container(ctx)->body.w * 0.14;
        mu_layout_row(ctx, 6, (int[]) { 80, sw, sw, sw, sw, -1 }, 0);
        for (int i = 0; colors[i].label; i++) {
            mu_label(ctx, colors[i].label);
            uint8_slider(ctx, &ctx->style->colors[i].r, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].g, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].b, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].a, 0, 255);
            mu_draw_rect(ctx, mu_layout_next(ctx), ctx->style->colors[i]);
        }
        mu_end_window(ctx);
    }
}

int64_t paint_time_ms = 0;
int64_t frame_budget_ms = 0;
int64_t sleep_time_ms = 0;

static void stats_window(mu_Context *ctx) {
    if (mu_begin_window_ex(ctx, "Stats", mu_rect(10, 10, 150, 128), MU_OPT_NOCLOSE | MU_OPT_NORESIZE)) {
        char buf[64];
        mu_layout_row(ctx, 2, (int[]) { 54, -1 }, 0);

        mu_label(ctx, "FPS:");
        sprintf(buf, "%d", (int)(1.0 / ((paint_time_ms > 0 ? paint_time_ms : 1) + (sleep_time_ms > 0 ? sleep_time_ms : 0)) * 1000.0));
        mu_label(ctx, buf);

        mu_label(ctx,"Time:");
        sprintf(buf, "%lld ms / frame", paint_time_ms);
        mu_label(ctx, buf);

        mu_label(ctx, "Budget:");
        sprintf(buf, "%lld ms", frame_budget_ms);
        mu_label(ctx, buf);

        mu_label(ctx, "Sleep:");
        sprintf(buf, "%lld ms", sleep_time_ms);
        mu_label(ctx, buf);

        mu_end_window(ctx);
    }
}

static void process_frame(mu_Context *ctx) {
    mu_begin(ctx);
    style_window(ctx);
    log_window(ctx);
    test_window(ctx);
    stats_window(ctx);
    mu_end(ctx);
}

static int text_width(mu_Font font, const char *text, int len) {
    if (len == -1) { len = (int)strlen(text); }
    return r_get_text_width(text, len);
}

static int text_height(mu_Font font) {
    return r_get_text_height();
}

static inline uint32_t r_color(mu_Color clr) {
    return ((uint32_t)clr.a << 24) | ((uint32_t)clr.r << 16) | ((uint32_t)clr.g << 8) | clr.b;
}

static void render_bg(struct fenster *window) {
    static struct point { float x; float y; } verts[3] = {
        {0, 100},
        {86.6, -50},
        {-86.6, -50},
    };

    static mu_Color vert_colors[3] = {
        {255, 0, 0, 255},
        {0, 255, 0, 255},
        {0, 0, 255, 255},
    };

    static mu_Color white = {255, 255, 255, 255};

    int w2 = window->width / 2;
    int h2 = window->height / 2;
    float theta = 3.14159f / 240.0;
    float cost = cos(theta);
    float sint = sin(theta);
    int offx = 180;

    for (int i = 0; i < 3; i++) {
        float x = verts[i].x;
        float y = verts[i].y;

        verts[i].x = x * cost - y * sint;
        verts[i].y = x * sint + y * cost;

//        r_line(window->x - offx, window->y, window->x + verts[i].x - offx, window->y - verts[i].y, r_color(vert_colors[i]));
//        r_wu_line(window->x + offx, window->y, window->x + verts[i].x + offx, window->y - verts[i].y, r_color(vert_colors[i]));

        r_draw_rect(mu_rect(w2 + verts[i].x - 5 - offx,
                            h2 - verts[i].y - 5, 10, 10), vert_colors[i]);

        r_draw_rect(mu_rect(w2 + verts[i].x - 5 + offx,
                            h2 - verts[i].y - 5, 10, 10), vert_colors[i]);
    }
    r_triangle((mu_Vec2){w2 + verts[0].x, h2 - verts[0].y}, vert_colors[0],
               (mu_Vec2){w2 + verts[1].x, h2 - verts[1].y}, vert_colors[1],
               (mu_Vec2){w2 + verts[2].x, h2 - verts[2].y}, vert_colors[2]);

//  for (int i = 0; i < 50; i++) {
////    r_wu_line(arc4random_uniform(window->width), arc4random_uniform(window->height), arc4random_uniform(window->width), arc4random_uniform(window->height), rand());
//    r_circle((mu_Vec2){arc4random_uniform(window->width), arc4random_uniform(window->height)}, arc4random_uniform(320), vert_colors[i % 3]);
//  }

    r_line(w2 + verts[0].x - offx, h2 - verts[0].y, w2 + verts[1].x - offx, h2 - verts[1].y, r_color(vert_colors[0]));
    r_line(w2 + verts[1].x - offx, h2 - verts[1].y, w2 + verts[2].x - offx, h2 - verts[2].y, r_color(vert_colors[1]));
    r_line(w2 + verts[2].x - offx, h2 - verts[2].y, w2 + verts[0].x - offx, h2 - verts[0].y, r_color(vert_colors[2]));

    r_wu_line(w2 + verts[0].x + offx, h2 - verts[0].y, w2 + verts[1].x + offx, h2 - verts[1].y, r_color(vert_colors[0]));
    r_wu_line(w2 + verts[1].x + offx, h2 - verts[1].y, w2 + verts[2].x + offx, h2 - verts[2].y, r_color(vert_colors[1]));
    r_wu_line(w2 + verts[2].x + offx, h2 - verts[2].y, w2 + verts[0].x + offx, h2 - verts[0].y, r_color(vert_colors[2]));

    r_draw_rect(mu_rect(w2 - 2 - offx, h2 - 2, 4, 4), white);
    r_draw_rect(mu_rect(w2 - 2 + offx, h2 - 2, 4, 4), white);
    
    static const char beanz[] = "FULL BEANZ";
    r_draw_text(beanz,
                (mu_Vec2){
                  window->width - r_get_text_width(beanz, (sizeof(beanz) / sizeof(*beanz)) - 1) - 10,
                  window->height - r_get_text_height() - 5
                },
                white);

    r_circle((mu_Vec2){w2, h2}, 100, white);
//    r_fill_circle((mu_Vec2){w2, h2 + 100}, 25, white);
}

int main(int argc, char **argv) {
    fenster_sleep(1000); // prevent stupid xcode from launching app twice.

    struct fenster window = {.title = "Full of beans: Hello World!", .width = 800, .height = 600};
    window.buf = calloc(window.width * window.height, sizeof(uint32_t));
    r_init((r_renderbuffer){.data = window.buf, .width = window.width, .height = window.height});

    fenster_open(&window);

    /* init microui */
    mu_Context *ctx = malloc(sizeof(mu_Context));
    mu_init(ctx);
    ctx->text_width = text_width;
    ctx->text_height = text_height;

    int fps = 60;
    bool mouse_pressed = false;

    enum key_state {
      KEY_UP,
      KEY_DOWN,
      KEY_CONSUMED,
    };

    enum mod_keys {
        MOD_CTRL  = 1 << 0,
        MOD_SHIFT = 1 << 1,
        MOD_ALT   = 1 << 2,
        MOD_META  = 1 << 3
    };

    /* main loop */
    while (true) {
        fenster_loop(&window); // swaps buffers too... maybe move after present()? also is not draining queued up events?

        // mouse motion
        mu_input_mousemove(ctx, window.x, window.y);
        
        // mouse scroll
        mu_input_scroll(ctx, -window.sx, -window.sy);

        // mouse up/down
        if (!mouse_pressed && window.mouse) {
            mu_input_mousedown(ctx, window.x, window.y, MU_MOUSE_LEFT);

        } else if (mouse_pressed && !window.mouse) {
            mu_input_mouseup(ctx, window.x, window.y, MU_MOUSE_LEFT);
        }
        mouse_pressed = window.mouse;

        // text input
        if (window.keys[0x1b] == KEY_DOWN) { // esc
            break;
        }

        // characters in NSEvent to get the actual typed chars...
        if (window.keys['\n'] == KEY_DOWN) {
            mu_input_keydown(ctx, MU_KEY_RETURN);

        } else if (window.keys['\n'] == KEY_UP) {
            mu_input_keyup(ctx, MU_KEY_RETURN);

        } 

        if (window.keys['\b'] == KEY_DOWN) {
            mu_input_keydown(ctx, MU_KEY_BACKSPACE);

        } else if (window.keys['\b'] == KEY_UP) {
            mu_input_keyup(ctx, MU_KEY_BACKSPACE);
        }

        if (window.keys['\t'] == KEY_DOWN) {
            mu_Container *c = mu_get_container(ctx, "Demo Window");
            c->open = !(c->open);

            c = mu_get_container(ctx, "Log Window");
            c->open = !(c->open);

            c = mu_get_container(ctx, "Style Editor");
            c->open = !(c->open);

        }

        // set modifiers...
        if (window.mod & MOD_CTRL) {
            mu_input_keydown(ctx, MU_KEY_CTRL);

        } else {
            mu_input_keyup(ctx, MU_KEY_CTRL);

        }

        if (window.mod & MOD_SHIFT) {
            mu_input_keydown(ctx, MU_KEY_SHIFT);

        } else {
            mu_input_keyup(ctx, MU_KEY_SHIFT);

        }

        if (window.mod & MOD_ALT) {
            mu_input_keydown(ctx, MU_KEY_ALT);

        } else {
            mu_input_keyup(ctx, MU_KEY_ALT);
        }

        // this could go in fenster at the top of the event loop...
        for (int i = 0; i < 256; i++) {
            if (window.keys[i] == KEY_DOWN && (' ' <= i  &&  i <= '~')) {
                char text[2] = { (window.mod & MOD_SHIFT) ? i : tolower(i) , 0};
                mu_input_text(ctx, text);
            }
            // if in the next loop there's no event, the key will show up as "consumed"
            window.keys[i] = KEY_CONSUMED;
        }

        int64_t before = fenster_time();

        /* process frame */
        process_frame(ctx);

        /* render */
        r_clear(mu_color(bg[0], bg[1], bg[2], 255));

        render_bg(&window);

        mu_Command *cmd = NULL;
        while (mu_next_command(ctx, &cmd)) {
            switch (cmd->type) {
                case MU_COMMAND_TEXT: r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
                case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
                case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
                case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect); break;
            }
        }
        r_present();

        int64_t after = fenster_time();
        paint_time_ms = after - before;
        frame_budget_ms = 1000 / fps;
        sleep_time_ms = frame_budget_ms - paint_time_ms;
        if (sleep_time_ms > 0) {
            fenster_sleep(sleep_time_ms);
        }
    }
    fenster_close(&window);

    return 0;
}
