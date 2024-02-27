/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2024 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

// This is the minimum required to get the RenderDoc demo UI to work
// The keyboard handling for delete is not perfect

/*
 * ==============================================================
 *
 *                              API
 *
 * ===============================================================
 */

#ifndef NK_APPKIT_H_
#define NK_APPKIT_H_

struct AppkitFont;
struct nk_appkit_window;

NK_API struct nk_context *nk_appkit_create(nk_appkit_window *win);
NK_API void nk_appkit_init(AppkitFont *font);
NK_API void nk_appkit_shutdown(void);
NK_API void nk_appkit_new_frame(void);
NK_API void nk_appkit_render(struct nk_color clear);

NK_API AppkitFont *nk_appkit_create_font(const char *name, int size);
NK_API void nk_appkit_delete_font(AppkitFont *font);

#endif    // #ifndef NK_APPKIT_H_

#if defined(NK_APPKIT_IMPLEMENTATION) || defined(NK_APPKIT_OBJC_IMPLEMENTATION)

#define nk_appkit_release 0
#define nk_appkit_press 1

#define nk_appkit_key_unknown -1

#define nk_appkit_key_0 48
#define nk_appkit_key_1 49
#define nk_appkit_key_2 50
#define nk_appkit_key_3 51
#define nk_appkit_key_4 52
#define nk_appkit_key_5 53
#define nk_appkit_key_6 54
#define nk_appkit_key_7 55
#define nk_appkit_key_8 56
#define nk_appkit_key_9 57

#define nk_appkit_key_A 65
#define nk_appkit_key_B 66
#define nk_appkit_key_C 67
#define nk_appkit_key_D 68
#define nk_appkit_key_E 69
#define nk_appkit_key_F 70
#define nk_appkit_key_G 71
#define nk_appkit_key_H 72
#define nk_appkit_key_I 73
#define nk_appkit_key_J 74
#define nk_appkit_key_K 75
#define nk_appkit_key_L 76
#define nk_appkit_key_M 77
#define nk_appkit_key_N 78
#define nk_appkit_key_O 79
#define nk_appkit_key_P 80
#define nk_appkit_key_Q 81
#define nk_appkit_key_R 82
#define nk_appkit_key_S 83
#define nk_appkit_key_T 84
#define nk_appkit_key_U 85
#define nk_appkit_key_V 86
#define nk_appkit_key_W 87
#define nk_appkit_key_X 88
#define nk_appkit_key_Y 89
#define nk_appkit_key_Z 90

#define nk_appkit_key_enter 301
#define nk_appkit_key_tab 302
#define nk_appkit_key_backspace 303
#define nk_appkit_key_delete 305
#define nk_appkit_key_right 306
#define nk_appkit_key_left 307
#define nk_appkit_key_down 308
#define nk_appkit_key_up 309
#define nk_appkit_key_page_up 310
#define nk_appkit_key_page_down 311
#define nk_appkit_key_home 312
#define nk_appkit_key_end 313

#define nk_appkit_key_left_shift 400
#define nk_appkit_key_left_control 401
#define nk_appkit_key_right_shift 402
#define nk_appkit_key_right_control 403

#define nk_appkit_key_first nk_appkit_key_0
#define nk_appkit_key_last nk_appkit_key_right_control

#define nk_appkit_mouse_button_left 0
#define nk_appkit_mouse_button_right 1
#define nk_appkit_mouse_button_middle 2

struct nk_appkit_window;

typedef void (*nk_appkit_mouse_button_cb)(nk_appkit_window *, int, int, int);
typedef void (*nk_appkit_character_cb)(nk_appkit_window *, unsigned int);
typedef void (*nk_appkit_key_cb)(nk_appkit_window *, int, int);
typedef void (*nk_appkit_scroll_cb)(nk_appkit_window *, double, double);

#endif    // #if defined(NK_APPKIT_IMPLEMENTATION) || defined(NK_APPKIT_OBJC_IMPLEMENTATION)

/*
 * ==============================================================
 *
 *                          IMPLEMENTATION
 *
 * ===============================================================
 */

#ifdef NK_APPKIT_IMPLEMENTATION

#include <mach/mach_time.h>

#ifndef NK_APPKIT_TEXT_MAX
#define NK_APPKIT_TEXT_MAX 256
#endif
#ifndef NK_APPKIT_DOUBLE_CLICK_LO
#define NK_APPKIT_DOUBLE_CLICK_LO 0.02
#endif
#ifndef NK_APPKIT_DOUBLE_CLICK_HI
#define NK_APPKIT_DOUBLE_CLICK_HI 0.2
#endif

struct AppkitFont
{
  struct nk_user_font nk;
  float height;
};

static struct nk_appkit
{
  nk_appkit_window *win;
  struct nk_context ctx;
  unsigned int text[NK_APPKIT_TEXT_MAX];
  int text_len;
  struct nk_vec2 scroll;
  double last_button_click;
  int is_double_click_down;
  struct nk_vec2 double_click_pos;
  uint64_t timerFrequency;
} nk_appkit;

bool nk_appkit_core_initialize(void);
void nk_appkit_core_shutdown(void);

nk_appkit_window *nk_appkit_window_create(int width, int height, const char *title);
void nk_appkit_window_delete(nk_appkit_window *window);
bool nk_appkit_window_is_closed(nk_appkit_window *window);
void nk_appkit_window_get_mouse_position(nk_appkit_window *window, double *xpos, double *ypos);
int nk_appkit_window_get_mouse_button_state(nk_appkit_window *window, int button);
int nk_appkit_window_get_key_state(nk_appkit_window *window, int key);

void nk_appkit_window_set_mouse_button_callback(nk_appkit_window *window,
                                                nk_appkit_mouse_button_cb callback);
void nk_appkit_window_set_scroll_callback(nk_appkit_window *window, nk_appkit_scroll_cb callback);
void nk_appkit_window_set_character_callback(nk_appkit_window *window,
                                             nk_appkit_character_cb callback);
void nk_appkit_window_set_key_callback(nk_appkit_window *window, nk_appkit_key_cb callback);

void nk_appkit_drawing_begin(nk_appkit_window *window, u_int8_t r, u_int8_t g, u_int8_t b,
                             u_int8_t a);
void nk_appkit_drawing_end(nk_appkit_window *window);

void nk_appkit_drawing_filled_rect(nk_appkit_window *window, short x, short y, u_int16_t w,
                                   u_int16_t h, u_int8_t r, u_int8_t g, u_int8_t b, u_int8_t a,
                                   int rounding);
void nk_appkit_drawing_rect(nk_appkit_window *window, short x, short y, u_int16_t w, u_int16_t h,
                            u_int8_t r, u_int8_t g, u_int8_t b, u_int8_t a, int rounding,
                            int lineThickness);
void nk_appkit_drawing_scissor(nk_appkit_window *window, short x, short y, u_int16_t w, u_int16_t h);
void nk_appkit_drawing_text(nk_appkit_window *window, short x, short y, u_int16_t w, u_int16_t h,
                            const char *text, int len, void *font, u_int8_t bgR, u_int8_t bgG,
                            u_int8_t bgB, u_int8_t bgA, u_int8_t fgR, u_int8_t fgG, u_int8_t fgB,
                            u_int8_t fgA);

float nk_appkit_drawing_set_font(nk_appkit_window *window, const char *name, float size);
float nk_appkit_drawing_get_text_width(nk_appkit_window *window, const char *text, int len);

static float nk_appkit_font_get_text_width(nk_handle handle, float height, const char *text, int len)
{
  AppkitFont *font = (AppkitFont *)handle.ptr;
  if(!font || !text)
    return 0.0f;

  return nk_appkit_drawing_get_text_width(nk_appkit.win, text, len);
}

static void nk_appkit_char_callback(nk_appkit_window *window, unsigned int codepoint)
{
  if(nk_appkit.text_len < NK_APPKIT_TEXT_MAX)
    nk_appkit.text[nk_appkit.text_len++] = codepoint;
}

static void nk_appkit_scroll_callback(nk_appkit_window *window, double xoff, double yoff)
{
  nk_appkit.scroll.x += (float)xoff;
  nk_appkit.scroll.y += (float)yoff;
}

static void nk_appkit_mouse_button_callback(nk_appkit_window *window, int button, int action, int mods)
{
  double x, y;
  if(button != nk_appkit_mouse_button_left)
    return;
  nk_appkit_window_get_mouse_position(window, &x, &y);
  if(action == nk_appkit_press)
  {
    double now = (double)mach_absolute_time() / nk_appkit.timerFrequency;
    double dt = now - nk_appkit.last_button_click;
    if(dt > NK_APPKIT_DOUBLE_CLICK_LO && dt < NK_APPKIT_DOUBLE_CLICK_HI)
    {
      nk_appkit.is_double_click_down = nk_true;
      nk_appkit.double_click_pos = nk_vec2((float)x, (float)y);
    }
    nk_appkit.last_button_click = now;
  }
  else
    nk_appkit.is_double_click_down = nk_false;
}

NK_API struct nk_context *nk_appkit_create(nk_appkit_window *win)
{
  nk_appkit.win = win;
  nk_appkit_window_set_character_callback(win, nk_appkit_char_callback);
  nk_appkit_window_set_scroll_callback(win, nk_appkit_scroll_callback);
  nk_appkit_window_set_mouse_button_callback(win, nk_appkit_mouse_button_callback);

  mach_timebase_info_data_t info;
  mach_timebase_info(&info);
  nk_appkit.timerFrequency = (info.denom * 1e9) / info.numer;

  nk_appkit.is_double_click_down = nk_false;
  nk_appkit.double_click_pos = nk_vec2(0, 0);
  nk_appkit.last_button_click = 0;

  return &nk_appkit.ctx;
}

NK_API void nk_appkit_init(AppkitFont *font)
{
  struct nk_user_font *nkFont = &font->nk;
  nkFont->userdata = nk_handle_ptr(font);
  nkFont->height = font->height;
  nkFont->width = nk_appkit_font_get_text_width;

  nk_init_default(&nk_appkit.ctx, nkFont);
  nk_appkit.ctx.clip.userdata = nk_handle_ptr(0);
}

NK_API void nk_appkit_shutdown(void)
{
  nk_free(&nk_appkit.ctx);
  memset(&nk_appkit, 0, sizeof(nk_appkit));
}

NK_API AppkitFont *nk_appkit_create_font(const char *name, int size)
{
  AppkitFont *font = (AppkitFont *)calloc(1, sizeof(AppkitFont));
  if(!font)
    return NULL;

  font->height = nk_appkit_drawing_set_font(nk_appkit.win, name, size);
  return font;
}

NK_API void nk_appkit_delete_font(AppkitFont *font)
{
  if(!font)
    return;
  free(font);
}

NK_API void nk_appkit_render(struct nk_color clear)
{
  const struct nk_command *cmd;
  struct nk_context *ctx = &nk_appkit.ctx;

  nk_appkit_drawing_begin(nk_appkit.win, clear.r, clear.g, clear.b, clear.a);
  nk_foreach(cmd, &nk_appkit.ctx)
  {
    switch(cmd->type)
    {
      case NK_COMMAND_NOP: break;
      case NK_COMMAND_SCISSOR:
      {
        const struct nk_command_scissor *s = (const struct nk_command_scissor *)cmd;
        nk_appkit_drawing_scissor(nk_appkit.win, s->x, s->y, s->w, s->h);
        break;
      }
      case NK_COMMAND_LINE: assert(false); break;
      case NK_COMMAND_CURVE: assert(false); break;
      case NK_COMMAND_RECT:
      {
        const struct nk_command_rect *r = (const struct nk_command_rect *)cmd;
        nk_appkit_drawing_rect(nk_appkit.win, r->x, r->y, r->w, r->h, r->color.r, r->color.g,
                               r->color.b, r->color.a, r->rounding, r->line_thickness);
        break;
      }
      case NK_COMMAND_RECT_FILLED:
      {
        const struct nk_command_rect_filled *r = (const struct nk_command_rect_filled *)cmd;
        nk_appkit_drawing_filled_rect(nk_appkit.win, r->x, r->y, r->w, r->h, r->color.r, r->color.g,
                                      r->color.b, r->color.a, r->rounding);
        break;
      }
      case NK_COMMAND_RECT_MULTI_COLOR: assert(false); break;
      case NK_COMMAND_CIRCLE: assert(false); break;
      case NK_COMMAND_CIRCLE_FILLED: assert(false); break;
      case NK_COMMAND_ARC: assert(false); break;
      case NK_COMMAND_ARC_FILLED: assert(false); break;
      case NK_COMMAND_TRIANGLE: assert(false); break;
      case NK_COMMAND_TRIANGLE_FILLED: assert(false); break;
      case NK_COMMAND_POLYGON: assert(false); break;
      case NK_COMMAND_POLYGON_FILLED: assert(false); break;
      case NK_COMMAND_POLYLINE: assert(false); break;
      case NK_COMMAND_TEXT:
      {
        const struct nk_command_text *t = (const struct nk_command_text *)cmd;
        nk_appkit_drawing_text(nk_appkit.win, t->x, t->y, t->w, t->h, (const char *)t->string,
                               t->length, (AppkitFont *)t->font->userdata.ptr, t->background.r,
                               t->background.g, t->background.b, t->background.a, t->foreground.r,
                               t->foreground.g, t->foreground.b, t->foreground.a);
        break;
      }
      case NK_COMMAND_IMAGE: assert(false); break;
      case NK_COMMAND_CUSTOM: assert(false); break;
    }
  }
  nk_appkit_drawing_end(nk_appkit.win);
  nk_clear(ctx);
}

NK_API void nk_appkit_new_frame(void)
{
  int i;
  double x, y;
  struct nk_context *ctx = &nk_appkit.ctx;
  nk_appkit_window *win = nk_appkit.win;

  nk_input_begin(ctx);
  for(i = 0; i < nk_appkit.text_len; ++i)
    nk_input_unicode(ctx, nk_appkit.text[i]);

  nk_input_key(ctx, NK_KEY_DEL,
               nk_appkit_window_get_key_state(win, nk_appkit_key_delete) == nk_appkit_press);
  nk_input_key(ctx, NK_KEY_ENTER,
               nk_appkit_window_get_key_state(win, nk_appkit_key_enter) == nk_appkit_press);
  nk_input_key(ctx, NK_KEY_TAB,
               nk_appkit_window_get_key_state(win, nk_appkit_key_tab) == nk_appkit_press);
  nk_input_key(ctx, NK_KEY_BACKSPACE,
               nk_appkit_window_get_key_state(win, nk_appkit_key_backspace) == nk_appkit_press);
  nk_input_key(ctx, NK_KEY_UP,
               nk_appkit_window_get_key_state(win, nk_appkit_key_up) == nk_appkit_press);
  nk_input_key(ctx, NK_KEY_DOWN,
               nk_appkit_window_get_key_state(win, nk_appkit_key_down) == nk_appkit_press);
  nk_input_key(ctx, NK_KEY_TEXT_START,
               nk_appkit_window_get_key_state(win, nk_appkit_key_home) == nk_appkit_press);
  nk_input_key(ctx, NK_KEY_TEXT_END,
               nk_appkit_window_get_key_state(win, nk_appkit_key_end) == nk_appkit_press);
  nk_input_key(ctx, NK_KEY_SCROLL_START,
               nk_appkit_window_get_key_state(win, nk_appkit_key_home) == nk_appkit_press);
  nk_input_key(ctx, NK_KEY_SCROLL_END,
               nk_appkit_window_get_key_state(win, nk_appkit_key_end) == nk_appkit_press);
  nk_input_key(ctx, NK_KEY_SCROLL_DOWN,
               nk_appkit_window_get_key_state(win, nk_appkit_key_page_down) == nk_appkit_press);
  nk_input_key(ctx, NK_KEY_SCROLL_UP,
               nk_appkit_window_get_key_state(win, nk_appkit_key_page_up) == nk_appkit_press);
  nk_input_key(ctx, NK_KEY_SHIFT,
               nk_appkit_window_get_key_state(win, nk_appkit_key_left_shift) == nk_appkit_press ||
                   nk_appkit_window_get_key_state(win, nk_appkit_key_right_shift) == nk_appkit_press);

  if(nk_appkit_window_get_key_state(win, nk_appkit_key_left_control) == nk_appkit_press ||
     nk_appkit_window_get_key_state(win, nk_appkit_key_right_control) == nk_appkit_press)
  {
    nk_input_key(ctx, NK_KEY_COPY,
                 nk_appkit_window_get_key_state(win, nk_appkit_key_C) == nk_appkit_press);

    nk_input_key(ctx, NK_KEY_PASTE,
                 nk_appkit_window_get_key_state(win, nk_appkit_key_V) == nk_appkit_press);
    nk_input_key(ctx, NK_KEY_CUT,
                 nk_appkit_window_get_key_state(win, nk_appkit_key_X) == nk_appkit_press);
    nk_input_key(ctx, NK_KEY_TEXT_UNDO,
                 nk_appkit_window_get_key_state(win, nk_appkit_key_Z) == nk_appkit_press);
    nk_input_key(ctx, NK_KEY_TEXT_REDO,
                 nk_appkit_window_get_key_state(win, nk_appkit_key_R) == nk_appkit_press);
    nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT,
                 nk_appkit_window_get_key_state(win, nk_appkit_key_left) == nk_appkit_press);
    nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT,
                 nk_appkit_window_get_key_state(win, nk_appkit_key_right) == nk_appkit_press);
    nk_input_key(ctx, NK_KEY_TEXT_LINE_START,
                 nk_appkit_window_get_key_state(win, nk_appkit_key_B) == nk_appkit_press);
    nk_input_key(ctx, NK_KEY_TEXT_LINE_END,
                 nk_appkit_window_get_key_state(win, nk_appkit_key_E) == nk_appkit_press);
  }
  else
  {
    nk_input_key(ctx, NK_KEY_LEFT,
                 nk_appkit_window_get_key_state(win, nk_appkit_key_left) == nk_appkit_press);
    nk_input_key(ctx, NK_KEY_RIGHT,
                 nk_appkit_window_get_key_state(win, nk_appkit_key_right) == nk_appkit_press);
    nk_input_key(ctx, NK_KEY_COPY, 0);
    nk_input_key(ctx, NK_KEY_PASTE, 0);
    nk_input_key(ctx, NK_KEY_CUT, 0);
    nk_input_key(ctx, NK_KEY_SHIFT, 0);
  }

  nk_appkit_window_get_mouse_position(win, &x, &y);
  nk_input_motion(ctx, (int)x, (int)y);
  nk_input_button(
      ctx, NK_BUTTON_LEFT, (int)x, (int)y,
      nk_appkit_window_get_mouse_button_state(win, nk_appkit_mouse_button_left) == nk_appkit_press);
  nk_input_button(ctx, NK_BUTTON_MIDDLE, (int)x, (int)y,
                  nk_appkit_window_get_mouse_button_state(win, nk_appkit_mouse_button_middle) ==
                      nk_appkit_press);
  nk_input_button(ctx, NK_BUTTON_RIGHT, (int)x, (int)y,
                  nk_appkit_window_get_mouse_button_state(win, nk_appkit_mouse_button_right) ==
                      nk_appkit_press);
  nk_input_button(ctx, NK_BUTTON_DOUBLE, (int)nk_appkit.double_click_pos.x,
                  (int)nk_appkit.double_click_pos.y, nk_appkit.is_double_click_down);
  nk_input_scroll(ctx, nk_appkit.scroll);
  nk_input_end(&nk_appkit.ctx);
  nk_appkit.text_len = 0;
  nk_appkit.scroll = nk_vec2(0, 0);
}

#endif    // #ifdef NK_APPKIT_IMPLEMENTATION

#ifdef NK_APPKIT_OBJC_IMPLEMENTATION

#import <AppKit/AppKit.h>

typedef struct nk_appkit_window
{
  NSWindow *nsWindow;
  id delegate;
  NSView *nsView;
  NSImage *nsImage;
  NSAutoreleasePool *nsPool;
  NSSize size;
  bool shouldClose;
  char mouseButtons[3];
  char keys[nk_appkit_key_last + 1];

  nk_appkit_character_cb characterCallback;
  nk_appkit_key_cb keyCallback;
  nk_appkit_mouse_button_cb mouseButtonCallback;
  nk_appkit_scroll_cb scrollCallback;
} nk_appkit_window;

typedef struct nk_appkit_state
{
  short int keycodes[256];
  nk_appkit_window *window;
  NSFont *nsFont;
  float fontHeight;
  CGEventSourceRef eventSource;
  id nsAppDelegate;
  id keyUpMonitor;
} nk_appkit_state;

static nk_appkit_state s_state = {0};

static void CreateKeyTables(void)
{
  memset(s_state.keycodes, -1, sizeof(s_state.keycodes));

  s_state.keycodes[0x1D] = nk_appkit_key_0;
  s_state.keycodes[0x12] = nk_appkit_key_1;
  s_state.keycodes[0x13] = nk_appkit_key_2;
  s_state.keycodes[0x14] = nk_appkit_key_3;
  s_state.keycodes[0x15] = nk_appkit_key_4;
  s_state.keycodes[0x17] = nk_appkit_key_5;
  s_state.keycodes[0x16] = nk_appkit_key_6;
  s_state.keycodes[0x1A] = nk_appkit_key_7;
  s_state.keycodes[0x1C] = nk_appkit_key_8;
  s_state.keycodes[0x19] = nk_appkit_key_9;

  s_state.keycodes[0x00] = nk_appkit_key_A;
  s_state.keycodes[0x0B] = nk_appkit_key_B;
  s_state.keycodes[0x08] = nk_appkit_key_C;
  s_state.keycodes[0x02] = nk_appkit_key_D;
  s_state.keycodes[0x0E] = nk_appkit_key_E;
  s_state.keycodes[0x03] = nk_appkit_key_F;
  s_state.keycodes[0x05] = nk_appkit_key_G;
  s_state.keycodes[0x04] = nk_appkit_key_H;
  s_state.keycodes[0x22] = nk_appkit_key_I;
  s_state.keycodes[0x26] = nk_appkit_key_J;
  s_state.keycodes[0x28] = nk_appkit_key_K;
  s_state.keycodes[0x25] = nk_appkit_key_L;
  s_state.keycodes[0x2E] = nk_appkit_key_M;
  s_state.keycodes[0x2D] = nk_appkit_key_N;
  s_state.keycodes[0x1F] = nk_appkit_key_O;
  s_state.keycodes[0x23] = nk_appkit_key_P;
  s_state.keycodes[0x0C] = nk_appkit_key_Q;
  s_state.keycodes[0x0F] = nk_appkit_key_R;
  s_state.keycodes[0x01] = nk_appkit_key_S;
  s_state.keycodes[0x11] = nk_appkit_key_T;
  s_state.keycodes[0x20] = nk_appkit_key_U;
  s_state.keycodes[0x09] = nk_appkit_key_V;
  s_state.keycodes[0x0D] = nk_appkit_key_W;
  s_state.keycodes[0x07] = nk_appkit_key_X;
  s_state.keycodes[0x10] = nk_appkit_key_Y;
  s_state.keycodes[0x06] = nk_appkit_key_Z;

  s_state.keycodes[0x33] = nk_appkit_key_backspace;
  s_state.keycodes[0x75] = nk_appkit_key_delete;
  s_state.keycodes[0x7D] = nk_appkit_key_down;
  s_state.keycodes[0x77] = nk_appkit_key_end;
  s_state.keycodes[0x24] = nk_appkit_key_enter;
  s_state.keycodes[0x73] = nk_appkit_key_home;
  s_state.keycodes[0x7B] = nk_appkit_key_left;
  s_state.keycodes[0x79] = nk_appkit_key_page_down;
  s_state.keycodes[0x74] = nk_appkit_key_page_up;
  s_state.keycodes[0x7C] = nk_appkit_key_right;
  s_state.keycodes[0x30] = nk_appkit_key_tab;
  s_state.keycodes[0x7E] = nk_appkit_key_up;

  s_state.keycodes[0x33] = nk_appkit_key_backspace;
  s_state.keycodes[0x75] = nk_appkit_key_delete;
  s_state.keycodes[0x7D] = nk_appkit_key_down;
  s_state.keycodes[0x77] = nk_appkit_key_end;
  s_state.keycodes[0x24] = nk_appkit_key_enter;
  s_state.keycodes[0x73] = nk_appkit_key_home;
  s_state.keycodes[0x7B] = nk_appkit_key_left;
  s_state.keycodes[0x3B] = nk_appkit_key_left_control;
  s_state.keycodes[0x38] = nk_appkit_key_left_shift;
  s_state.keycodes[0x79] = nk_appkit_key_page_down;
  s_state.keycodes[0x74] = nk_appkit_key_page_up;
  s_state.keycodes[0x7C] = nk_appkit_key_right;
  s_state.keycodes[0x3E] = nk_appkit_key_right_control;
  s_state.keycodes[0x3C] = nk_appkit_key_right_shift;
  s_state.keycodes[0x30] = nk_appkit_key_tab;
  s_state.keycodes[0x7E] = nk_appkit_key_up;
}

static int TranslateKey(unsigned int key)
{
  if(key >= sizeof(s_state.keycodes) / sizeof(s_state.keycodes[0]))
    return nk_appkit_key_unknown;

  return s_state.keycodes[key];
}

static void InputMouseClick(nk_appkit_window *window, int button, int action)
{
  if(button < 0 || button > 2)
    return;

  window->mouseButtons[button] = (char)action;
  if(window->mouseButtonCallback)
    window->mouseButtonCallback(window, button, action, 0);
}

static void InputKey(nk_appkit_window *window, int key, int action)
{
  if(key >= 0 && key <= nk_appkit_key_last)
  {
    if(action == nk_appkit_release && window->keys[key] == nk_appkit_release)
      return;

    window->keys[key] = (char)action;
  }
}

@interface nk_appkit_windowDelegate : NSObject
{
  nk_appkit_window *window;
}

- (instancetype)init_with_window:(nk_appkit_window *)initWindow;

@end

@implementation nk_appkit_windowDelegate

- (instancetype)init_with_window:(nk_appkit_window *)initWindow
{
  self = [super init];
  window = initWindow;
  return self;
}

- (BOOL)windowShouldClose:(id)sender
{
  window->shouldClose = true;
  return NO;
}
@end

@interface nk_appkit_windowView : NSView<NSTextInputClient>
{
  nk_appkit_window *window;
}

- (instancetype)init_with_window:(nk_appkit_window *)initWindow;

@end

@implementation nk_appkit_windowView

- (instancetype)init_with_window:(nk_appkit_window *)initWindow
{
  self = [super init];
  window = initWindow;
  return self;
}

- (void)drawRect:(NSRect)dirtyRect
{
  if(window->nsImage != nil)
    [window->nsImage drawInRect:dirtyRect];
}

- (void)mouseDown:(NSEvent *)event
{
  InputMouseClick(window, nk_appkit_mouse_button_left, nk_appkit_press);
}

- (void)mouseUp:(NSEvent *)event
{
  InputMouseClick(window, nk_appkit_mouse_button_left, nk_appkit_release);
}

- (void)rightMouseDown:(NSEvent *)event
{
  InputMouseClick(window, nk_appkit_mouse_button_right, nk_appkit_press);
}

- (void)rightMouseUp:(NSEvent *)event
{
  InputMouseClick(window, nk_appkit_mouse_button_right, nk_appkit_release);
}

- (void)otherMouseDown:(NSEvent *)event
{
  InputMouseClick(window, (int)[event buttonNumber], nk_appkit_press);
}

- (void)otherMouseUp:(NSEvent *)event
{
  InputMouseClick(window, (int)[event buttonNumber], nk_appkit_release);
}

- (void)keyDown:(NSEvent *)event
{
  const int key = TranslateKey([event keyCode]);
  InputKey(window, key, nk_appkit_press);
  [self interpretKeyEvents:@[ event ]];
  if(window->keyCallback)
    window->keyCallback(window, key, nk_appkit_press);
}

- (void)flagsChanged:(NSEvent *)event
{
  int action;
  const unsigned int modifierFlags =
      [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;
  const int key = TranslateKey([event keyCode]);
  NSUInteger keyFlag = 0;
  switch(key)
  {
    case nk_appkit_key_left_shift:
    case nk_appkit_key_right_shift: keyFlag = NSEventModifierFlagShift; break;
    case nk_appkit_key_left_control:
    case nk_appkit_key_right_control: keyFlag = NSEventModifierFlagControl; break;
  }

  if(keyFlag & modifierFlags)
  {
    if(window->keys[key] == nk_appkit_press)
      action = nk_appkit_release;
    else
      action = nk_appkit_press;
  }
  else
    action = nk_appkit_release;

  InputKey(window, key, action);
}

- (void)keyUp:(NSEvent *)event
{
  const int key = TranslateKey([event keyCode]);
  InputKey(window, key, nk_appkit_release);
  if(window->keyCallback)
    window->keyCallback(window, key, nk_appkit_release);
}

- (void)scrollWheel:(NSEvent *)event
{
  double deltaX = [event scrollingDeltaX];
  double deltaY = [event scrollingDeltaY];

  if([event hasPreciseScrollingDeltas])
  {
    deltaX *= 0.1;
    deltaY *= 0.1;
  }

  if(fabs(deltaX) > 0.0 || fabs(deltaY) > 0.0)
  {
    if(window->scrollCallback)
      window->scrollCallback(window, deltaX, deltaY);
  }
}

static const NSRange s_NSRange_Empty = {NSNotFound, 0};

- (BOOL)hasMarkedText
{
  return NO;
}

- (NSRange)markedRange
{
  return s_NSRange_Empty;
}

- (NSRange)selectedRange
{
  return s_NSRange_Empty;
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange
{
}

- (void)unmarkText
{
}

- (NSArray *)validAttributesForMarkedText
{
  return [NSArray array];
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
                                                actualRange:(NSRangePointer)actualRange
{
  return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point
{
  return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange
{
  const NSRect frame = [window->nsView frame];
  return NSMakeRect(frame.origin.x, frame.origin.y, 0.0, 0.0);
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange
{
  if(!window->characterCallback)
    return;

  NSString *characters;
  NSEvent *event = [NSApp currentEvent];

  if([string isKindOfClass:[NSAttributedString class]])
    characters = [string string];
  else
    characters = (NSString *)string;

  NSRange range = NSMakeRange(0, [characters length]);
  while(range.length)
  {
    uint32_t codepoint = 0;

    if([characters getBytes:&codepoint
                  maxLength:sizeof(codepoint)
                 usedLength:nil
                   encoding:NSUTF32StringEncoding
                    options:0
                      range:range
             remainingRange:&range])
    {
      if(codepoint >= 0xf700 && codepoint <= 0xf7ff)
        continue;

      if(window->characterCallback)
        window->characterCallback(window, codepoint);
    }
  }
}

@end

void nk_appkit_window_get_mouse_position(nk_appkit_window *window, double *xpos, double *ypos)
{
  @autoreleasepool
  {
    const NSRect contentRect = [window->nsView frame];
    const NSPoint pos = [window->nsWindow mouseLocationOutsideOfEventStream];

    if(xpos)
      *xpos = pos.x;
    if(ypos)
      *ypos = contentRect.size.height - pos.y;
  }
}

@interface nk_appkit_ApplicationDelegate : NSObject<NSApplicationDelegate>
@end

@implementation nk_appkit_ApplicationDelegate

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
  if(s_state.window)
    s_state.window->shouldClose = true;
  return NSTerminateCancel;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
  @autoreleasepool
  {
    NSEvent *event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                        location:NSMakePoint(0, 0)
                                   modifierFlags:0
                                       timestamp:0
                                    windowNumber:0
                                         context:nil
                                         subtype:0
                                           data1:0
                                           data2:0];
    [NSApp postEvent:event atStart:YES];
    [NSApp stop:nil];
  }
}

@end    // nk_appkit_ApplicationDelegate

bool nk_appkit_window_is_closed(nk_appkit_window *window)
{
  return window->shouldClose;
}

void nk_appkit_window_set_character_callback(nk_appkit_window *window, nk_appkit_character_cb callback)
{
  window->characterCallback = callback;
}

void nk_appkit_window_set_key_callback(nk_appkit_window *window, nk_appkit_key_cb callback)
{
  window->keyCallback = callback;
}

void nk_appkit_window_set_scroll_callback(nk_appkit_window *window, nk_appkit_scroll_cb callback)
{
  window->scrollCallback = callback;
}

void nk_appkit_window_set_mouse_button_callback(nk_appkit_window *window,
                                                nk_appkit_mouse_button_cb callback)
{
  window->mouseButtonCallback = callback;
}

int nk_appkit_window_get_key_state(nk_appkit_window *window, int key)
{
  assert(key >= nk_appkit_key_first && key <= nk_appkit_key_last);
  return (int)window->keys[key];
}

int nk_appkit_window_get_mouse_button_state(nk_appkit_window *window, int button)
{
  assert(button >= 0 && button <= 2);
  return (int)window->mouseButtons[button];
}

void nk_appkit_window_delete(nk_appkit_window *window)
{
  assert(window == s_state.window);

  window->characterCallback = nil;
  window->keyCallback = nil;
  window->mouseButtonCallback = nil;
  window->scrollCallback = nil;

  @autoreleasepool
  {
    [window->nsWindow orderOut:nil];
    [window->nsWindow setDelegate:nil];

    [window->delegate release];
    window->delegate = nil;

    [window->nsImage release];
    window->nsImage = nil;

    [window->nsView release];
    window->nsView = nil;

    [window->nsWindow close];
    window->nsWindow = nil;
  }
  window->nsPool = nil;

  free(window);
  s_state.window = NULL;
}

void nk_appkit_core_shutdown(void)
{
  if(s_state.window)
    nk_appkit_window_delete(s_state.window);

  @autoreleasepool
  {
    if(s_state.eventSource)
    {
      CFRelease(s_state.eventSource);
      s_state.eventSource = nil;
    }

    if(s_state.nsAppDelegate)
    {
      [NSApp setDelegate:nil];
      [s_state.nsAppDelegate release];
      s_state.nsAppDelegate = nil;
    }

    if(s_state.keyUpMonitor)
      [NSEvent removeMonitor:s_state.keyUpMonitor];

    if(s_state.nsFont)
    {
      [s_state.nsFont release];
      s_state.nsFont = nil;
    }
  }

  memset(&s_state, 0, sizeof(s_state));
}

bool nk_appkit_core_initialize(void)
{
  memset(&s_state, 0, sizeof(s_state));

  @autoreleasepool
  {
    [NSApplication sharedApplication];

    s_state.nsAppDelegate = [nk_appkit_ApplicationDelegate new];
    [NSApp setDelegate:s_state.nsAppDelegate];

    NSEvent * (^block)(NSEvent *) = ^NSEvent *(NSEvent *event)
    {
      if([event modifierFlags] & NSEventModifierFlagCommand)
        [[NSApp keyWindow] sendEvent:event];
      return event;
    };

    s_state.keyUpMonitor = [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyUp
                                                                 handler:block];

    CreateKeyTables();

    s_state.eventSource = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if(!s_state.eventSource)
    {
      nk_appkit_core_shutdown();
      return false;
    }

    CGEventSourceSetLocalEventsSuppressionInterval(s_state.eventSource, 0.0);

    if(![[NSRunningApplication currentApplication] isFinishedLaunching])
      [NSApp run];

    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  }

  return true;
}

nk_appkit_window *nk_appkit_window_create(int width, int height, const char *title)
{
  nk_appkit_window *window = (nk_appkit_window *)calloc(1, sizeof(nk_appkit_window));
  s_state.window = window;

  window->delegate = [[nk_appkit_windowDelegate alloc] init_with_window:window];

  window->nsWindow = [[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, width, height)
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable
                  backing:NSBackingStoreBuffered
                    defer:NO];

  [(NSWindow *)window->nsWindow center];

  window->nsView = [[nk_appkit_windowView alloc] init_with_window:window];

  [window->nsWindow setContentView:window->nsView];
  [window->nsWindow makeFirstResponder:window->nsView];
  [window->nsWindow setTitle:@(title)];
  [window->nsWindow setDelegate:window->delegate];
  [window->nsWindow setAcceptsMouseMovedEvents:YES];
  [window->nsWindow setRestorable:NO];

  [window->nsWindow orderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];
  [window->nsWindow makeKeyAndOrderFront:nil];

  window->size = NSMakeSize(width, height);
  window->nsImage = [[NSImage alloc] initWithSize:window->size];

  return (nk_appkit_window *)window;
}

void nk_appkit_drawing_begin(nk_appkit_window *window, u_int8_t r, u_int8_t g, u_int8_t b, u_int8_t a)
{
  window->nsPool = [[NSAutoreleasePool alloc] init];
  [NSGraphicsContext saveGraphicsState];
  [window->nsImage lockFocus];
  [[NSColor colorWithRed:r / 255.0f green:g / 255.0f blue:b / 255.0f alpha:a / 255.0f] setFill];
  NSRectFill(NSMakeRect(0, 0, window->size.width, window->size.height));
}

void nk_appkit_drawing_end(nk_appkit_window *window)
{
  [window->nsImage unlockFocus];
  [window->nsView setNeedsDisplay:TRUE];
  [NSGraphicsContext restoreGraphicsState];
  [window->nsPool drain];
}

float nk_appkit_drawing_set_font(nk_appkit_window *window, const char *name, float size)
{
  @autoreleasepool
  {
    s_state.nsFont = [NSFont fontWithName:[NSString stringWithUTF8String:name] size:size];
    [s_state.nsFont retain];
    NSLayoutManager *nsLayoutManager = [NSLayoutManager new];
    float height = [nsLayoutManager defaultLineHeightForFont:s_state.nsFont];
    s_state.fontHeight = height + 1;
    [nsLayoutManager autorelease];
    return s_state.fontHeight;
  }
}

float nk_appkit_drawing_get_text_width(nk_appkit_window *window, const char *text, int len)
{
  @autoreleasepool
  {
    NSString *nsString = [[NSString alloc] initWithBytes:(void *)text
                                                  length:len
                                                encoding:NSASCIIStringEncoding];
    NSMutableDictionary *atts = [NSMutableDictionary new];
    [atts setObject:s_state.nsFont forKey:NSFontAttributeName];
    NSSize bounds = [nsString sizeWithAttributes:atts];
    float width = floor(bounds.width);
    [atts autorelease];
    [nsString autorelease];
    return width;
  }
}

void nk_appkit_drawing_filled_rect(nk_appkit_window *window, short x, short y, u_int16_t w,
                                   u_int16_t h, u_int8_t r, u_int8_t g, u_int8_t b, u_int8_t a,
                                   int rounding)
{
  @autoreleasepool
  {
    [[NSColor colorWithRed:r / 255.0f green:g / 255.0f blue:b / 255.0f alpha:a / 255.0f] setFill];
    float H = window->size.height;
    float y0 = H - y - h;
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(x, y0, w, h)
                                                         xRadius:rounding
                                                         yRadius:rounding];
    [path fill];
  }
}

void nk_appkit_drawing_rect(nk_appkit_window *window, short x, short y, u_int16_t w, u_int16_t h,
                            u_int8_t r, u_int8_t g, u_int8_t b, u_int8_t a, int rounding,
                            int lineThickness)
{
  @autoreleasepool
  {
    [[NSColor colorWithRed:r / 255.0f green:g / 255.0f blue:b / 255.0f alpha:a / 255.0f] setStroke];
    float H = window->size.height;
    float y0 = H - y - h;
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSMakeRect(x, y0, w, h)
                                                         xRadius:rounding
                                                         yRadius:rounding];
    path.lineWidth = lineThickness;
    [path stroke];
  }
}

void nk_appkit_drawing_scissor(nk_appkit_window *window, short x, short y, u_int16_t w, u_int16_t h)
{
  @autoreleasepool
  {
    [NSGraphicsContext restoreGraphicsState];
    [NSGraphicsContext saveGraphicsState];
    float H = window->size.height;
    float y0 = H - y - h;
    NSRectClip(NSMakeRect(x, y0, w, h));
  }
}

void nk_appkit_drawing_text(nk_appkit_window *window, short x, short y, u_int16_t w, u_int16_t h,
                            const char *text, int len, void *font, u_int8_t bgR, u_int8_t bgG,
                            u_int8_t bgB, u_int8_t bgA, u_int8_t fgR, u_int8_t fgG, u_int8_t fgB,
                            u_int8_t fgA)
{
  @autoreleasepool
  {
    float H = window->size.height;
    float y0 = H - y - s_state.fontHeight;
    NSString *nsString = [[NSString alloc] initWithBytes:(void *)text
                                                  length:len
                                                encoding:NSASCIIStringEncoding];
    NSMutableDictionary *atts = [NSMutableDictionary new];
    [atts setObject:s_state.nsFont forKey:NSFontAttributeName];
    [atts setObject:[NSColor colorWithRed:bgR / 255.0f
                                    green:bgG / 255.0f
                                     blue:bgB / 255.0f
                                    alpha:bgA / 255.0f]
             forKey:NSBackgroundColorAttributeName];
    [atts setObject:[NSColor colorWithRed:fgR / 255.0f
                                    green:fgG / 255.0f
                                     blue:fgB / 255.0f
                                    alpha:fgA / 255.0f]
             forKey:NSForegroundColorAttributeName];
    [nsString drawAtPoint:NSMakePoint(x, y0) withAttributes:atts];
    [atts autorelease];
    [nsString autorelease];
  }
}
#endif    // #ifdef NK_APPKIT_OBJC_IMPLEMENTATION
