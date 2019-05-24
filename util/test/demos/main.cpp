/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include <stdio.h>
#include <string.h>
#include <string>

#include "test_common.h"

#pragma warning(push)
#pragma warning(disable : 4127)    // conditional expression is constant
#pragma warning(disable : 4244)    // conversion from 'x' to 'y', possible loss of data
#pragma warning(disable : 4505)    // unreferenced local function has been removed
#pragma warning(disable : 4701)    // potentially uninitialized local variable used

#define NK_IMPLEMENTATION
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_ASSERT(expr) TEST_ASSERT(expr, "nuklear assertion failed")
#include "3rdparty/nuklear/nuklear.h"

#if defined(WIN32)

#define NK_GDI_IMPLEMENTATION
#include "3rdparty/nuklear/nuklear_gdi.h"

static LRESULT CALLBACK NuklearWndProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
  if(msg == WM_CLOSE)
  {
    DestroyWindow(wnd);
    return 0;
  }
  if(msg == WM_DESTROY)
  {
    PostQuitMessage(0);
    return 0;
  }
  if(msg == WM_KEYDOWN && wparam == VK_ESCAPE)
  {
    PostQuitMessage(0);
    return 0;
  }

  if(nk_gdi_handle_event(wnd, msg, wparam, lparam))
    return 0;

  return DefWindowProcW(wnd, msg, wparam, lparam);
}

WNDCLASSW wc = {};
HWND wnd = NULL;
HDC dc = NULL;
GdiFont *font = NULL;

nk_context *NuklearInit(int width, int height, const char *title)
{
  wc.style = CS_DBLCLKS;
  wc.lpfnWndProc = NuklearWndProc;
  wc.hInstance = GetModuleHandleA(NULL);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = L"NuklearWindowClass";
  RegisterClassW(&wc);

  int len = (int)strlen(title);

  int wsize = MultiByteToWideChar(CP_UTF8, 0, title, len, NULL, 0);
  WCHAR *wstr = (WCHAR *)_alloca(wsize * sizeof(wchar_t) + 2);
  wstr[wsize] = 0;
  MultiByteToWideChar(CP_UTF8, 0, title, len, wstr, wsize);

  RECT rect = {0, 0, width, height};
  AdjustWindowRectEx(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, FALSE,
                     WS_EX_WINDOWEDGE);
  wnd = CreateWindowExW(
      WS_EX_WINDOWEDGE, wc.lpszClassName, wstr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
      (GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2,
      (GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2, rect.right - rect.left,
      rect.bottom - rect.top, NULL, NULL, wc.hInstance, NULL);

  dc = GetDC(wnd);

  font = nk_gdifont_create("Arial", 14);
  return nk_gdi_init(font, dc, width, height);
}

bool NuklearTick(nk_context *ctx)
{
  MSG msg = {};
  UpdateWindow(wnd);

  nk_input_begin(ctx);
  // Check to see if any messages are waiting in the queue
  while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
  {
    // Translate the message and dispatch it to WindowProc()
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  nk_input_end(ctx);

  if(!IsWindowVisible(wnd) || msg.message == WM_QUIT)
    return false;

  return true;
}

void NuklearRender()
{
  nk_gdi_render(nk_rgb(30, 30, 30));
}

void NuklearShutdown()
{
  nk_gdifont_del(font);
  ReleaseDC(wnd, dc);
  DestroyWindow(wnd);
  UnregisterClassW(wc.lpszClassName, wc.hInstance);
}

#else

#define NK_XLIB_IMPLEMENTATION
#include "3rdparty/nuklear/nuklear_xlib.h"

Display *dpy = NULL;
Colormap cmap = 0;
Window win = 0;
int screen = 0;
XFont *font = NULL;

nk_context *NuklearInit(int width, int height, const char *title)
{
  dpy = XOpenDisplay(NULL);
  if(!dpy)
    return NULL;
  Window root = DefaultRootWindow(dpy);
  screen = XDefaultScreen(dpy);
  Visual *vis = XDefaultVisual(dpy, screen);
  cmap = XCreateColormap(dpy, root, vis, AllocNone);

  XSetWindowAttributes swa = {};
  swa.colormap = cmap;
  swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPress | ButtonReleaseMask |
                   ButtonMotionMask | Button1MotionMask | Button3MotionMask | Button4MotionMask |
                   Button5MotionMask | PointerMotionMask | KeymapStateMask;
  win = XCreateWindow(dpy, root, 0, 0, width, height, 0, XDefaultDepth(dpy, screen), InputOutput,
                      vis, CWEventMask | CWColormap, &swa);

  XStoreName(dpy, win, title);
  XMapWindow(dpy, win);
  Atom wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(dpy, win, &wm_delete_window, 1);

  /* GUI */
  font = nk_xfont_create(dpy, "fixed");
  return nk_xlib_init(font, dpy, screen, win, width, height);
}

bool NuklearTick(nk_context *ctx)
{
  XEvent evt;

  nk_input_begin(ctx);
  while(XPending(dpy))
  {
    XNextEvent(dpy, &evt);
    if(evt.type == ClientMessage)
      return false;
    if(XFilterEvent(&evt, win))
      continue;
    nk_xlib_handle_event(dpy, screen, win, &evt);
  }
  nk_input_end(ctx);

  return true;
}

void NuklearRender()
{
  XClearWindow(dpy, win);
  nk_xlib_render(win, nk_rgb(30, 30, 30));
  XFlush(dpy);
}

void NuklearShutdown()
{
  nk_xfont_del(dpy, font);
  nk_xlib_shutdown();
  XUnmapWindow(dpy, win);
  XFreeColormap(dpy, cmap);
  XDestroyWindow(dpy, win);
  XCloseDisplay(dpy);
}

#endif

// nuklear
#pragma warning(pop)

std::vector<TestMetadata> &test_list()
{
  static std::vector<TestMetadata> list;
  return list;
}

void check_tests(int argc, char **argv)
{
  std::vector<TestMetadata> &tests = test_list();

  for(TestMetadata &test : tests)
    test.test->Prepare(argc, argv);
}

void RegisterTest(TestMetadata test)
{
  test_list().push_back(test);
}

int main(int argc, char **argv)
{
  std::vector<TestMetadata> &tests = test_list();

  std::sort(tests.begin(), tests.end());

  if(argc >= 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h") || !strcmp(argv[1], "-?") ||
                   !strcmp(argv[1], "/help") || !strcmp(argv[1], "/h") || !strcmp(argv[1], "/?")))
  {
    printf(R"(RenderDoc testing demo program

Usage: %s Test_Name [test_options]

  --help                        Print this help message.
  --list                        Lists all tests, with name, API, description, availability.
  --list-available              Lists the available test names only, one per line.
  --validate
  --debug                       Run the demo with API validation enabled.
  --gpu [identifier]            Try to select the corresponding GPU where available and possible
                                through the API. Identifier is e.g. 'nv' or 'amd', or can be '1080'
  --warp                        On D3D APIs, use the software rasterizer.
  --width / -w                  Specify the window width.
  --height / -h                 Specify the window height.
  --frames <n>
  --max-frames <n>
  --frame-count <n>             Only run the demo for this number of frames
  --data <path>                 Specfiy where extended data should come from.
                                By default in the path in $RENDERDOC_DEMOS_DATA
                                environment variable, or else in the data/demos
                                folder next to the executable.
)",
           argc == 0 ? "demos" : argv[0]);

    fflush(stdout);
    return 1;
  }

  if(argc >= 2 && !strcmp(argv[1], "--list"))
  {
    check_tests(argc, argv);

    TestAPI prev = TestAPI::Count;

    for(const TestMetadata &test : tests)
    {
      if(test.API != prev)
      {
        if(prev != TestAPI::Count)
          printf("\n\n");
        printf("======== %s tests ========\n\n", APIName(test.API));
      }

      prev = test.API;

      printf("%s: %s", test.Name, test.IsAvailable() ? "Available" : "Unavailable");

      if(!test.IsAvailable())
        printf(" because %s", test.AvailMessage());

      printf("\n\t%s\n\n", test.Description);
    }

    fflush(stdout);
    return 1;
  }

  if(argc >= 2 && !strcmp(argv[1], "--list-raw"))
  {
    check_tests(argc, argv);

    // output TSV
    printf("Name\tAvailable\tAvailMessage\n");

    for(const TestMetadata &test : tests)
    {
      printf("%s\t%s\t%s\n", test.Name, test.IsAvailable() ? "True" : "False",
             test.IsAvailable() ? "Available" : test.AvailMessage());
    }

    fflush(stdout);
    return 1;
  }

  if(tests.empty())
  {
    fprintf(stderr, "No tests registered\n");
    fflush(stderr);
    return 1;
  }

  std::string testchoice;

  if(argc >= 2)
  {
    testchoice = argv[1];
  }
  else
  {
    check_tests(argc, argv);

    const int width = 400, height = 575;

    nk_context *ctx = NuklearInit(width, height, "RenderDoc Test Program");

    if(!ctx)
      return 1;

    int curtest = 0;
    bool allow[(int)TestAPI::Count] = {};
    const char *allow_names[] = {
        "D3D11", "Vulkan", "OpenGL", "D3D12",
    };

    for(size_t i = 0; i < ARRAY_COUNT(allow); i++)
      allow[i] = true;

    char name_filter[256] = {};

    static_assert(ARRAY_COUNT(allow) == ARRAY_COUNT(allow_names), "Mismatched array");

    while(NuklearTick(ctx))
    {
      if(nk_begin(ctx, "Demo", nk_rect(0, 0, (float)width, (float)height), NK_WINDOW_NO_SCROLLBAR))
      {
        nk_layout_row_dynamic(ctx, 100, 1);
        if(nk_group_begin(ctx, "Test Filter", NK_WINDOW_BORDER | NK_WINDOW_TITLE))
        {
          nk_layout_row_dynamic(ctx, 30, ARRAY_COUNT(allow_names) + 1);

          nk_label(ctx, "Filter tests:", NK_TEXT_LEFT);

          for(size_t i = 0; i < ARRAY_COUNT(allow); i++)
          {
            std::string text = allow_names[i];

            allow[i] = nk_check_label(ctx, text.c_str(), allow[i]) != 0;
          }

          nk_group_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 270, 1);
        if(nk_group_begin(ctx, "Test", NK_WINDOW_BORDER | NK_WINDOW_TITLE))
        {
          nk_layout_row_begin(ctx, NK_STATIC, 20, 2);
          nk_layout_row_push(ctx, 60.0f);
          nk_label(ctx, "Test Filter:", NK_TEXT_ALIGN_MIDDLE | NK_TEXT_ALIGN_RIGHT);
          nk_layout_row_push(ctx, 280.0f);
          nk_edit_string_zero_terminated(ctx, NK_EDIT_FIELD, name_filter, 256, NULL);
          nk_layout_row_end(ctx);

          float prevSpacing = 0;
          std::swap(prevSpacing, ctx->style.window.spacing.y);

          nk_layout_row_dynamic(ctx, 20, 1);

          std::string lower_filter = strlower(name_filter);

          for(int i = 0; i < (int)tests.size(); i++)
          {
            if(!tests[i].IsAvailable())
              continue;

            std::string lower_name = strlower(tests[i].Name);

            // apply filters
            if(!allow[(int)tests[i].API] ||
               (!lower_filter.empty() && !strstr(lower_name.c_str(), lower_filter.c_str())))
            {
              // if this was the selected test, unselect it. The next unfiltered test will grab it
              if(curtest == i)
                curtest = -1;

              continue;
            }

            // grab the current test, if none is selected (see above)
            if(curtest == -1)
              curtest = i;

            if(nk_select_label(ctx, tests[i].Name, NK_TEXT_LEFT, curtest == i))
              curtest = i;
          }

          std::swap(prevSpacing, ctx->style.window.spacing.y);
          nk_group_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 150, 1);

        TestMetadata &selected_test = tests[curtest >= 0 ? curtest : 0];

        if(nk_group_begin(ctx, "Test Information", NK_WINDOW_BORDER | NK_WINDOW_TITLE))
        {
          if(curtest >= 0)
          {
            nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 2);
            nk_layout_row_push(ctx, 0.25f);
            nk_label(ctx, "Test name:", NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_RIGHT);
            nk_layout_row_push(ctx, 0.75f);
            nk_label(ctx, selected_test.Name, NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_LEFT);
            nk_layout_row_end(ctx);

            nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 2);
            nk_layout_row_push(ctx, 0.25f);
            nk_label(ctx, "API:", NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_RIGHT);
            nk_layout_row_push(ctx, 0.75f);
            nk_label(ctx, APIName(selected_test.API), NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_LEFT);
            nk_layout_row_end(ctx);

            nk_layout_row_begin(ctx, NK_DYNAMIC, 0, 2);
            nk_layout_row_push(ctx, 0.25f);
            nk_label(ctx, "Description:", NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_RIGHT);
            nk_layout_row_push(ctx, 0.75f);
            nk_label_wrap(ctx, selected_test.Description);
            nk_layout_row_end(ctx);
          }
          else
          {
            nk_layout_row_begin(ctx, NK_DYNAMIC, 20, 1);
            nk_layout_row_push(ctx, 0.25f);
            nk_label(ctx, "No test selected", NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_CENTERED);
            nk_layout_row_end(ctx);
          }

          nk_group_end(ctx);
        }

        nk_layout_row_dynamic(ctx, 30, 1);

        if(curtest >= 0)
        {
          if(nk_button_label(ctx, "Run"))
          {
            testchoice = selected_test.Name;
            break;
          }
        }
        else
        {
          nk_label(ctx, "No test selected", NK_TEXT_ALIGN_TOP | NK_TEXT_ALIGN_CENTERED);
        }
      }
      nk_end(ctx);

      NuklearRender();
    }

    NuklearShutdown();
  }

  if(testchoice.empty())
    return 0;

  for(const TestMetadata &test : tests)
  {
    if(testchoice == test.Name)
    {
      TEST_LOG("\n\n======\nRunning %s\n\n", test.Name);
      test.test->Prepare(argc, argv);

      if(!test.IsAvailable())
      {
        TEST_ERROR("%s is not available: %s", test.Name, test.test->Avail.c_str());
        return 5;
      }

      int ret = test.test->main();
      test.test->Shutdown();
      return ret;
    }
  }

  TEST_ERROR("%s is not a known test", argv[1]);

  return 2;
}

#if defined(WIN32)
int WINAPI wWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine,
                    _In_ int nShowCmd)
{
  LPWSTR *wargv;
  int argc;

  if(AttachConsole(ATTACH_PARENT_PROCESS))
  {
    FILE *dummy = NULL;
    freopen_s(&dummy, "CONOUT$", "w", stdout);
    freopen_s(&dummy, "CONOUT$", "w", stderr);
  }

  wargv = CommandLineToArgvW(GetCommandLineW(), &argc);

  char **argv = new char *[argc];
  for(int i = 0; i < argc; i++)
  {
    // allocate pessimistically
    int allocSize = (int)wcslen(wargv[i]) * 4 + 1;

    argv[i] = new char[allocSize];

    WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, &argv[i][0], allocSize, NULL, NULL);
  }

  main(argc, argv);

  delete[] argv;
  LocalFree(wargv);
}

#endif
