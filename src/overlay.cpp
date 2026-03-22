#include "overlay.h"

#include "common.h"

namespace mycross {
namespace {

AppContext* g_app = nullptr;

RECT overlay_rect(const Config& cfg) {
  const int screen_width = GetSystemMetrics(SM_CXSCREEN);
  const int screen_height = GetSystemMetrics(SM_CYSCREEN);
  int center_x = cfg.x < 0 ? screen_width / 2 : cfg.x;
  int center_y = cfg.y < 0 ? screen_height / 2 : cfg.y;
  center_x = clampi(center_x, 0, screen_width - 1);
  center_y = clampi(center_y, 0, screen_height - 1);

  RECT rect = {};
  rect.left = center_x - cfg.window_size / 2;
  rect.top = center_y - cfg.window_size / 2;
  rect.right = rect.left + cfg.window_size;
  rect.bottom = rect.top + cfg.window_size;
  return rect;
}

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  if (g_app == nullptr) {
    return DefWindowProcW(hwnd, message, wparam, lparam);
  }

  switch (message) {
    case WM_PAINT: {
      Config cfg;
      {
        std::lock_guard<std::mutex> lock(g_app->state.mu);
        cfg = g_app->state.cfg;
      }

      PAINTSTRUCT ps = {};
      HDC dc = BeginPaint(hwnd, &ps);
      HPEN pen = CreatePen(PS_SOLID, cfg.line_width,
                           RGB(cfg.color_r, cfg.color_g, cfg.color_b));
      HGDIOBJ old_pen = SelectObject(dc, pen);
      const int center_x = cfg.window_size / 2;
      const int center_y = cfg.window_size / 2;
      MoveToEx(dc, center_x - cfg.cross_half, center_y, nullptr);
      LineTo(dc, center_x + cfg.cross_half, center_y);
      MoveToEx(dc, center_x, center_y - cfg.cross_half, nullptr);
      LineTo(dc, center_x, center_y + cfg.cross_half);
      SelectObject(dc, old_pen);
      DeleteObject(pen);
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_DESTROY:
      if (hwnd == g_app->overlay_wnd) {
        g_app->overlay_wnd = nullptr;
      }
      return 0;
    default:
      return DefWindowProcW(hwnd, message, wparam, lparam);
  }
}

LRESULT CALLBACK CtlProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  if (g_app == nullptr) {
    return DefWindowProcW(hwnd, message, wparam, lparam);
  }

  switch (message) {
    case WM_APP_SYNC:
      apply_overlay(*g_app);
      return 0;
    case WM_HOTKEY:
      if (wparam == HOTKEY_ID) {
        {
          std::lock_guard<std::mutex> lock(g_app->state.mu);
          g_app->state.running = false;
        }
        apply_overlay(*g_app);
        return 0;
      }
      break;
    case WM_APP_EXIT:
      PostQuitMessage(0);
      return 0;
    case WM_DESTROY:
      UnregisterHotKey(hwnd, HOTKEY_ID);
      PostQuitMessage(0);
      return 0;
  }

  return DefWindowProcW(hwnd, message, wparam, lparam);
}

void overlay_thread_main(AppContext* app) {
  g_app = app;

  WNDCLASSW overlay_class = {};
  overlay_class.lpfnWndProc = OverlayProc;
  overlay_class.hInstance = app->inst;
  overlay_class.lpszClassName = OV_CLASS;
  overlay_class.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
  RegisterClassW(&overlay_class);

  WNDCLASSW control_class = {};
  control_class.lpfnWndProc = CtlProc;
  control_class.hInstance = app->inst;
  control_class.lpszClassName = CTL_CLASS;
  RegisterClassW(&control_class);

  app->ctl_wnd = CreateWindowW(CTL_CLASS, L"", WS_OVERLAPPED, 0, 0, 0, 0, nullptr,
                               nullptr, app->inst, nullptr);
  RegisterHotKey(app->ctl_wnd, HOTKEY_ID, HOTKEY_MOD, HOTKEY_VK);
  app->overlay_ready = true;

  MSG msg = {};
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  if (app->overlay_wnd) {
    DestroyWindow(app->overlay_wnd);
    app->overlay_wnd = nullptr;
  }
  if (app->ctl_wnd) {
    DestroyWindow(app->ctl_wnd);
    app->ctl_wnd = nullptr;
  }
  app->overlay_ready = false;
  g_app = nullptr;
}

}  // namespace

void apply_overlay(AppContext& app) {
  Config cfg;
  bool running = false;
  {
    std::lock_guard<std::mutex> lock(app.state.mu);
    cfg = app.state.cfg;
    running = app.state.running;
  }

  if (!running) {
    if (app.overlay_wnd) {
      DestroyWindow(app.overlay_wnd);
      app.overlay_wnd = nullptr;
    }
    return;
  }

  const RECT rect = overlay_rect(cfg);
  if (!app.overlay_wnd) {
    app.overlay_wnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
        OV_CLASS, L"", WS_POPUP, rect.left, rect.top, rect.right - rect.left,
        rect.bottom - rect.top, nullptr, nullptr, app.inst, nullptr);
    if (!app.overlay_wnd) {
      return;
    }
    SetLayeredWindowAttributes(app.overlay_wnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    ShowWindow(app.overlay_wnd, SW_SHOW);
  } else {
    MoveWindow(app.overlay_wnd, rect.left, rect.top, rect.right - rect.left,
               rect.bottom - rect.top, TRUE);
  }
  InvalidateRect(app.overlay_wnd, nullptr, TRUE);
}

void post_sync(const AppContext& app) {
  if (app.ctl_wnd) {
    PostMessageW(app.ctl_wnd, WM_APP_SYNC, 0, 0);
  }
}

void start_overlay(AppContext& app) {
  app.overlay_thread = std::thread(overlay_thread_main, &app);
  for (int i = 0; i < 300 && !app.overlay_ready.load(); ++i) {
    Sleep(10);
  }
}

void stop_overlay(AppContext& app) {
  if (app.ctl_wnd) {
    PostMessageW(app.ctl_wnd, WM_APP_EXIT, 0, 0);
  }
  if (app.overlay_thread.joinable()) {
    app.overlay_thread.join();
  }
}

}  // namespace mycross
