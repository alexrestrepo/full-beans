/*
** Copyright (c) 2024 Serge Zaitsev
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to
** deal in the Software without restriction, including without limitation the
** rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
** sell copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
** FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
** IN THE SOFTWARE.
*/

#ifndef FENSTER_H
#define FENSTER_H

#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#include <objc/NSObjCRuntime.h>
#include <objc/objc-runtime.h>
#elif defined(_WIN32)
#include <windows.h>
#else
#define _DEFAULT_SOURCE 1
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <time.h>
#endif

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct fenster {
  const char *title;
  const int width;
  const int height;
  uint32_t *buf;
  int keys[256]; /* keys are mostly ASCII, but arrows are 17..20 */
  int mod;       /* mod is 4 bits mask, ctrl=1, shift=2, alt=4, meta=8 */
  int x;
  int y;
  int mouse;
  float sx;
  float sy;
#if defined(__APPLE__)
  id wnd;
#elif defined(_WIN32)
  HWND hwnd;
#else
  Display *dpy;
  Window w;
  GC gc;
  XImage *img;
#endif
};

#ifndef FENSTER_API
#define FENSTER_API extern
#endif
FENSTER_API int fenster_open(struct fenster *f);
FENSTER_API int fenster_loop(struct fenster *f);
FENSTER_API void fenster_close(struct fenster *f);
FENSTER_API void fenster_sleep(int64_t ms);
FENSTER_API int64_t fenster_time(void);
#define fenster_pixel(f, x, y) ((f)->buf[((y) * (f)->width) + (x)])

#ifndef FENSTER_HEADER
#if defined(__APPLE__)
#define msg(r, o, s) ((r(*)(id, SEL))objc_msgSend)(o, sel_getUid(s))
#define msg1(r, o, s, A, a)                                                    \
  ((r(*)(id, SEL, A))objc_msgSend)(o, sel_getUid(s), a)
#define msg2(r, o, s, A, a, B, b)                                              \
  ((r(*)(id, SEL, A, B))objc_msgSend)(o, sel_getUid(s), a, b)
#define msg3(r, o, s, A, a, B, b, C, c)                                        \
  ((r(*)(id, SEL, A, B, C))objc_msgSend)(o, sel_getUid(s), a, b, c)
#define msg4(r, o, s, A, a, B, b, C, c, D, d)                                  \
  ((r(*)(id, SEL, A, B, C, D))objc_msgSend)(o, sel_getUid(s), a, b, c, d)

#define cls(x) ((id)objc_getClass(x))

#define nstr(x) (msg1(id, cls("NSString"), "stringWithUTF8String:", const char *, x))
#define nsel(x) (NSSelectorFromString(nstr(x)))

extern id const NSDefaultRunLoopMode;
extern id const NSApp;
extern SEL NSSelectorFromString(id aSelectorName);

static inline void setupMenu(void) {
  id mainMenu = msg(id, msg(id, cls("NSMenu"), "alloc"), "init");
  
  // Application Menu
  id appMenuItem = msg(id, msg(id, cls("NSMenuItem"), "alloc"), "init");
  id appMenu = msg(id, msg(id, cls("NSMenu"), "alloc"), "init");
  id appName = msg(id, msg(id, cls("NSProcessInfo"), "processInfo"), "processName");
  
  // About Item
  id title = msg1(id, nstr("About "), "stringByAppendingString:", id, appName);
  SEL sel = nsel("orderFrontStandardAboutPanel:");
  id aboutItem = msg3(id, msg(id, cls("NSMenuItem"), "alloc"), "initWithTitle:action:keyEquivalent:", id, title, SEL, sel, id, nstr(""));
  
  // Quit Item
  title = msg1(id, nstr("Quit "), "stringByAppendingString:", id, appName);
  sel = nsel("terminate:");
  id quitItem =  msg3(id, msg(id, cls("NSMenuItem"), "alloc"), "initWithTitle:action:keyEquivalent:", id, title, SEL, sel, id, nstr("q"));
  
  // setup main menu
  msg1(void, appMenu, "addItem:", id, aboutItem);
  msg1(void, appMenu, "addItem:", id, msg(id, cls("NSMenuItem"), "separatorItem"));
  msg1(void, appMenu, "addItem:", id, quitItem);
  
  msg1(void, appMenuItem, "setSubmenu:", id, appMenu);
  msg1(void, mainMenu, "addItem:", id, appMenuItem);
  
  msg1(void, NSApp, "setMainMenu:", id, mainMenu);
}

static void applicationDidFinishLaunching(id self, SEL _cmd, id note) {
  setupMenu();
  msg1(void, NSApp, "activateIgnoringOtherApps:", BOOL, YES);
}

static void applicationWillTerminate(id self, SEL _cmd, id note) {
//  printf("bye...\n");
}

static BOOL viewAcceptsFirstResponder(id self, SEL _cmd) {
  return YES;
}

static void viewKeyDown(id self, SEL _cmd, id event) {
  // prevent beeps
}

static Class winDelegateClass = NULL;
static Class fensterViewClass = NULL;

static BOOL fenster_should_close(id v, SEL s, id w) {
  (void)v, (void)s, (void)w;
  msg1(void, NSApp, "terminate:", id, NSApp);
  return YES;
}

static void fenster_draw_rect(id v, SEL s, CGRect r) {
  (void)r, (void)s;
  struct fenster *f = (struct fenster *)objc_getAssociatedObject(v, "fenster");
  CGContextRef context =
  msg(CGContextRef, msg(id, cls("NSGraphicsContext"), "currentContext"),
    "graphicsPort");
  CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
  CGDataProviderRef provider = CGDataProviderCreateWithData(
    NULL, f->buf, f->width * f->height * 4, NULL);
  CGImageRef img =
  CGImageCreate(f->width, f->height, 8, 32, f->width * 4, space,
    kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Little,
    provider, NULL, false, kCGRenderingIntentDefault);
  CGColorSpaceRelease(space);
  CGDataProviderRelease(provider);
  CGContextSetInterpolationQuality(context, kCGInterpolationNone);
  CGContextDrawImage(context, CGRectMake(0, 0, f->width, f->height), img);
  CGImageRelease(img);
}

static inline void initialize(void) {
  static BOOL initialized = NO;
  if (!initialized) {
    msg(id, cls("NSApplication"), "sharedApplication");
    msg1(void, NSApp, "setActivationPolicy:", NSInteger, 0); // NSApplicationActivationPolicyRegular
    
    Class appdelegate = objc_allocateClassPair((Class)cls("NSObject"), "AppDelegate", 0);
    class_addMethod(appdelegate, sel_getUid("applicationDidFinishLaunching:"), (IMP)applicationDidFinishLaunching, "v@:@");
    class_addMethod(appdelegate, sel_getUid("applicationWillTerminate:"), (IMP)applicationWillTerminate, "v@:@");
    objc_registerClassPair(appdelegate);
    msg1(void, NSApp, "setDelegate:", id,
      msg(id, msg(id, (id)appdelegate, "alloc"), "init"));
    
    winDelegateClass = objc_allocateClassPair((Class)cls("NSObject"), "FensterDelegate", 0);
    class_addMethod(winDelegateClass, sel_getUid("windowShouldClose:"), (IMP)fenster_should_close, "c@:@");
    objc_registerClassPair(winDelegateClass);
    
    fensterViewClass = objc_allocateClassPair((Class)cls("NSView"), "FensterView", 0);
    class_addMethod(fensterViewClass, sel_getUid("drawRect:"), (IMP)fenster_draw_rect, "v@:@@");
    class_addMethod(fensterViewClass, sel_getUid("acceptsFirstResponder"), (IMP)viewAcceptsFirstResponder, "c@");
    class_addMethod(fensterViewClass, sel_getUid("keyDown:"), (IMP)viewKeyDown, "v@:@");
    objc_registerClassPair(fensterViewClass);
    
    msg(void, NSApp, "finishLaunching");
    initialized = YES;
  }
}

FENSTER_API int fenster_open(struct fenster *f) {
  initialize();
  
  f->wnd = msg4(id, msg(id, cls("NSWindow"), "alloc"),
    "initWithContentRect:styleMask:backing:defer:",
    CGRect, CGRectMake(0, 0, f->width, f->height),
    NSUInteger, 3, // NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
    NSUInteger, 2, // NSBackingStoreBuffered
    BOOL, NO);

  msg1(void, f->wnd, "setDelegate:", id,
    msg(id, msg(id, (id)winDelegateClass, "alloc"), "init"));
  
  id view = msg(id, msg(id, (id)fensterViewClass, "alloc"), "init");
  msg1(void, f->wnd, "setContentView:", id, view);
  msg1(void, f->wnd, "makeFirstResponder:", id, view);
  objc_setAssociatedObject(view, "fenster", (id)f, OBJC_ASSOCIATION_ASSIGN);
  
  id title = nstr(f->title);
  
  msg1(void, f->wnd, "setTitle:", id, title);
  msg1(void, f->wnd, "makeKeyAndOrderFront:", id, nil);
  msg(void, f->wnd, "center");  
  return 0;
}

FENSTER_API void fenster_close(struct fenster *f) {
  msg(void, f->wnd, "close");
}

// clang-format off
static const uint8_t FENSTER_KEYCODES[128] = {65,83,68,70,72,71,90,88,67,86,0,66,81,87,69,82,89,84,49,50,51,52,54,53,61,57,55,45,56,48,93,79,85,91,73,80,10,76,74,39,75,59,92,44,47,78,77,46,9,32,96,8,0,27,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,26,2,3,127,0,5,0,4,0,20,19,18,17,0};
// clang-format on
FENSTER_API int fenster_loop(struct fenster *f) {
  msg1(void, msg(id, f->wnd, "contentView"), "setNeedsDisplay:", BOOL, YES);
  id event = msg4(id, NSApp,
    "nextEventMatchingMask:untilDate:inMode:dequeue:",
    NSUInteger, NSUIntegerMax,  // nextEventMatchingMask:NSEventMaskAny
    id, NULL,                   // untilDate: nil
    id, NSDefaultRunLoopMode,   // inMode: NSDefaultRunLoopMode
    BOOL, YES);                 // dequeue: YES

  if (!event) {
    return 0;
  }

  NSUInteger evtype = msg(NSUInteger, event, "type");
  switch (evtype) {
    case 1: /* NSEventTypeMouseDown */
      f->mouse |= 1;
      break;
    case 2: /* NSEventTypeMouseUp*/
      f->mouse &= ~1;
      break;
    case 5: /* NSEventTypeMouseMoved */
    case 6: { /* NSEventTypeMouseDragged */
      CGPoint xy = msg(CGPoint, event, "locationInWindow");
      f->x = (int)xy.x;
      f->y = (int)(f->height - xy.y);
      break;
    }
    case 10: /*NSEventTypeKeyDown*/
    case 11: /*NSEventTypeKeyUp*/ {
      NSUInteger k = msg(NSUInteger, event, "keyCode");
      f->keys[k < 127 ? FENSTER_KEYCODES[k] : 0] = evtype == 10;
    }
    // fallthrough
    case 12: /*NSEventTypeFlagsChanged*/ {
      NSUInteger mod = msg(NSUInteger, event, "modifierFlags") >> 17;
      f->mod = (int)((mod & 0xc) | ((mod & 1) << 1) | ((mod >> 1) & 1));
      break;
    }
    case 22: /*NSEventTypeScrollWheel*/ {
      CGFloat dy = msg(CGFloat, event, "scrollingDeltaY");
      CGFloat dx = msg(CGFloat, event, "scrollingDeltaX");
      f->sy = (float)dy;
      f->sx = (float)dx;
      break;
    }
  }
  msg1(void, NSApp, "sendEvent:", id, event);
  msg(void, NSApp, "updateWindows");
  return 0;
}
#elif defined(_WIN32)
// clang-format off
static const uint8_t FENSTER_KEYCODES[] = {0,27,49,50,51,52,53,54,55,56,57,48,45,61,8,9,81,87,69,82,84,89,85,73,79,80,91,93,10,0,65,83,68,70,71,72,74,75,76,59,39,96,0,92,90,88,67,86,66,78,77,44,46,47,0,0,0,32,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,17,3,0,20,0,19,0,5,18,4,26,127};
// clang-format on
typedef struct BINFO{
    BITMAPINFOHEADER    bmiHeader;
    RGBQUAD             bmiColors[3];
}BINFO;
static LRESULT CALLBACK fenster_wndproc(HWND hwnd, UINT msg, WPARAM wParam,
                                        LPARAM lParam) {
  struct fenster *f = (struct fenster *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  switch (msg) {
  case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    HDC memdc = CreateCompatibleDC(hdc);
    HBITMAP hbmp = CreateCompatibleBitmap(hdc, f->width, f->height);
    HBITMAP oldbmp = SelectObject(memdc, hbmp);
    BINFO bi = {{sizeof(bi), f->width, -f->height, 1, 32, BI_BITFIELDS}};
    bi.bmiColors[0].rgbRed = 0xff;
    bi.bmiColors[1].rgbGreen = 0xff;
    bi.bmiColors[2].rgbBlue = 0xff;
    SetDIBitsToDevice(memdc, 0, 0, f->width, f->height, 0, 0, 0, f->height,
                      f->buf, (BITMAPINFO *)&bi, DIB_RGB_COLORS);
    BitBlt(hdc, 0, 0, f->width, f->height, memdc, 0, 0, SRCCOPY);
    SelectObject(memdc, oldbmp);
    DeleteObject(hbmp);
    DeleteDC(memdc);
    EndPaint(hwnd, &ps);
  } break;
  case WM_CLOSE:
    DestroyWindow(hwnd);
    break;
  case WM_LBUTTONDOWN:
  case WM_LBUTTONUP:
    f->mouse = (msg == WM_LBUTTONDOWN);
    break;
  case WM_MOUSEMOVE:
    f->y = HIWORD(lParam), f->x = LOWORD(lParam);
    break;
  case WM_KEYDOWN:
  case WM_KEYUP: {
    f->mod = ((GetKeyState(VK_CONTROL) & 0x8000) >> 15) |
             ((GetKeyState(VK_SHIFT) & 0x8000) >> 14) |
             ((GetKeyState(VK_MENU) & 0x8000) >> 13) |
             (((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) >> 12);
    f->keys[FENSTER_KEYCODES[HIWORD(lParam) & 0x1ff]] = !((lParam >> 31) & 1);
  } break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  return 0;
}

FENSTER_API int fenster_open(struct fenster *f) {
  HINSTANCE hInstance = GetModuleHandle(NULL);
  WNDCLASSEX wc = {0};
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = CS_VREDRAW | CS_HREDRAW;
  wc.lpfnWndProc = fenster_wndproc;
  wc.hInstance = hInstance;
  wc.lpszClassName = f->title;
  RegisterClassEx(&wc);
  f->hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, f->title, f->title,
                           WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                           f->width, f->height, NULL, NULL, hInstance, NULL);

  if (f->hwnd == NULL)
    return -1;
  SetWindowLongPtr(f->hwnd, GWLP_USERDATA, (LONG_PTR)f);
  ShowWindow(f->hwnd, SW_NORMAL);
  UpdateWindow(f->hwnd);
  return 0;
}

FENSTER_API void fenster_close(struct fenster *f) { (void)f; }

FENSTER_API int fenster_loop(struct fenster *f) {
  MSG msg;
  while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    if (msg.message == WM_QUIT)
      return -1;
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  InvalidateRect(f->hwnd, NULL, TRUE);
  return 0;
}
#else
// clang-format off
static int FENSTER_KEYCODES[124] = {XK_BackSpace,8,XK_Delete,127,XK_Down,18,XK_End,5,XK_Escape,27,XK_Home,2,XK_Insert,26,XK_Left,20,XK_Page_Down,4,XK_Page_Up,3,XK_Return,10,XK_Right,19,XK_Tab,9,XK_Up,17,XK_apostrophe,39,XK_backslash,92,XK_bracketleft,91,XK_bracketright,93,XK_comma,44,XK_equal,61,XK_grave,96,XK_minus,45,XK_period,46,XK_semicolon,59,XK_slash,47,XK_space,32,XK_a,65,XK_b,66,XK_c,67,XK_d,68,XK_e,69,XK_f,70,XK_g,71,XK_h,72,XK_i,73,XK_j,74,XK_k,75,XK_l,76,XK_m,77,XK_n,78,XK_o,79,XK_p,80,XK_q,81,XK_r,82,XK_s,83,XK_t,84,XK_u,85,XK_v,86,XK_w,87,XK_x,88,XK_y,89,XK_z,90,XK_0,48,XK_1,49,XK_2,50,XK_3,51,XK_4,52,XK_5,53,XK_6,54,XK_7,55,XK_8,56,XK_9,57};
// clang-format on
FENSTER_API int fenster_open(struct fenster *f) {
  f->dpy = XOpenDisplay(NULL);
  int screen = DefaultScreen(f->dpy);
  f->w = XCreateSimpleWindow(f->dpy, RootWindow(f->dpy, screen), 0, 0, f->width,
                             f->height, 0, BlackPixel(f->dpy, screen),
                             WhitePixel(f->dpy, screen));
  f->gc = XCreateGC(f->dpy, f->w, 0, 0);
  XSelectInput(f->dpy, f->w,
               ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                   ButtonReleaseMask | PointerMotionMask);
  XStoreName(f->dpy, f->w, f->title);
  XMapWindow(f->dpy, f->w);
  XSync(f->dpy, f->w);
  f->img = XCreateImage(f->dpy, DefaultVisual(f->dpy, 0), 24, ZPixmap, 0,
                        (char *)f->buf, f->width, f->height, 32, 0);
  return 0;
}
FENSTER_API void fenster_close(struct fenster *f) { XCloseDisplay(f->dpy); }
FENSTER_API int fenster_loop(struct fenster *f) {
  XEvent ev;
  XPutImage(f->dpy, f->w, f->gc, f->img, 0, 0, 0, 0, f->width, f->height);
  XFlush(f->dpy);
  while (XPending(f->dpy)) {
    XNextEvent(f->dpy, &ev);
    switch (ev.type) {
    case ButtonPress:
    case ButtonRelease:
      f->mouse = (ev.type == ButtonPress);
      break;
    case MotionNotify:
      f->x = ev.xmotion.x, f->y = ev.xmotion.y;
      break;
    case KeyPress:
    case KeyRelease: {
      int m = ev.xkey.state;
      int k = XkbKeycodeToKeysym(f->dpy, ev.xkey.keycode, 0, 0);
      for (unsigned int i = 0; i < 124; i += 2) {
        if (FENSTER_KEYCODES[i] == k) {
          f->keys[FENSTER_KEYCODES[i + 1]] = (ev.type == KeyPress);
          break;
        }
      }
      f->mod = (!!(m & ControlMask)) | (!!(m & ShiftMask) << 1) |
               (!!(m & Mod1Mask) << 2) | (!!(m & Mod4Mask) << 3);
    } break;
    }
  }
  return 0;
}
#endif

#ifdef _WIN32
FENSTER_API void fenster_sleep(int64_t ms) { Sleep(ms); }
FENSTER_API int64_t fenster_time() {
  LARGE_INTEGER freq, count;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&count);
  return (int64_t)(count.QuadPart * 1000.0 / freq.QuadPart);
}
#else
FENSTER_API void fenster_sleep(int64_t ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000;
  nanosleep(&ts, NULL);
}
FENSTER_API int64_t fenster_time(void) {
  struct timespec time;
  clock_gettime(CLOCK_REALTIME, &time);
  return time.tv_sec * 1000 + (time.tv_nsec / 1000000);
}
#endif

#ifdef __cplusplus
class Fenster {
  struct fenster f;
  int64_t now;

public:
  Fenster(const int w, const int h, const char *title)
      : f{.title = title, .width = w, .height = h} {
    this->f.buf = new uint32_t[w * h];
    clear();
    this->now = fenster_time();
    fenster_open(&this->f);
  }
  ~Fenster() {
    fenster_close(&this->f);
    delete[] this->f.buf;
  }
  void clear() { memset(this->f.buf, 0, this->f.width * this->f.height * 4); }
  bool loop(const int fps) {
    int64_t t = fenster_time();
    int64_t paint_time = t - this->now;
    int64_t frame_budget = 1000 / fps;
    int64_t sleep_time = frame_budget - paint_time;
    if (sleep_time > 0) {
      fenster_sleep(sleep_time);
    }
    this->now = t;
    return fenster_loop(&this->f) == 0;
  }
  void paint() { fenster_loop(&this->f); }
  inline uint32_t &px(const int x, const int y) {
    return fenster_pixel(&this->f, x, y);
  }
  bool key(int c) { return c >= 0 && c < 128 ? this->f.keys[c] : false; }
  int x() { return this->f.x; }
  int y() { return this->f.y; }
  int mouse() { return this->f.mouse; }
  int mod() { return this->f.mod; }
  long width() { return this->f.width; }
  long height() { return this->f.height; }
};
#endif /* __cplusplus */

#endif /* !FENSTER_HEADER */
#endif /* FENSTER_H */
