#include "ui_launcher.h"

#include "config_store.h"

namespace mycross {
namespace {

std::wstring edge_path() {
  const std::wstring path64 =
      L"C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe";
  const std::wstring path32 =
      L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe";
  if (GetFileAttributesW(path64.c_str()) != INVALID_FILE_ATTRIBUTES) {
    return path64;
  }
  if (GetFileAttributesW(path32.c_str()) != INVALID_FILE_ATTRIBUTES) {
    return path32;
  }
  return L"msedge.exe";
}

}  // namespace

bool launch_ui(AppContext& app) {
  app.launch_error = 0;
  const std::wstring exe = edge_path();
  std::wstring cmd =
      L"\"" + exe + L"\" --app=http://127.0.0.1:5188/ --new-window --window-size=1120,700";
  STARTUPINFOW startup_info = {};
  startup_info.cb = sizeof(startup_info);
  ZeroMemory(&app.ui_proc, sizeof(app.ui_proc));
  if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      nullptr, &startup_info, &app.ui_proc)) {
    app.launch_error = GetLastError();
    return false;
  }
  return true;
}

void stop_ui(AppContext& app) {
  if (app.ui_proc.hProcess) {
    const DWORD wait = WaitForSingleObject(app.ui_proc.hProcess, 150);
    if (wait == WAIT_TIMEOUT) {
      TerminateProcess(app.ui_proc.hProcess, 0);
    }
    CloseHandle(app.ui_proc.hProcess);
    app.ui_proc.hProcess = nullptr;
  }
  if (app.ui_proc.hThread) {
    CloseHandle(app.ui_proc.hThread);
    app.ui_proc.hThread = nullptr;
  }
}

void apply_cli(AppContext& app) {
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  if (!argv) {
    return;
  }

  Config cfg;
  {
    std::lock_guard<std::mutex> lock(app.state.mu);
    cfg = app.state.cfg;
  }

  if (argc >= 3) {
    cfg.x = _wtoi(argv[1]);
    cfg.y = _wtoi(argv[2]);
  }
  for (int i = 1; i < argc; ++i) {
    if (wcsncmp(argv[i], L"--x=", 4) == 0) {
      cfg.x = _wtoi(argv[i] + 4);
    } else if (wcsncmp(argv[i], L"--y=", 4) == 0) {
      cfg.y = _wtoi(argv[i] + 4);
    }
  }

  normalize(cfg);
  {
    std::lock_guard<std::mutex> lock(app.state.mu);
    app.state.cfg = cfg;
  }
  LocalFree(argv);
}

}  // namespace mycross
