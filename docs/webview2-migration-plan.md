# MyCross WebView2 迁移方案

## 目标

把当前项目从：

- `MinGW + g++ + 外部 Edge app 窗口`

迁移到：

- `MSVC + WebView2 + 自己的 Win32 主窗口`

迁移完成后：

- 窗口大小由程序自己控制，不再依赖 `--window-size`
- 保留现有 HTML/CSS/JS 前端
- 保留本地 HTTP 服务架构
- 整体行为更像正式桌面应用

## 总体顺序

1. 准备 MSVC 和 WebView2 开发环境
2. 先让现有项目在 MSVC 下编译通过
3. 新增 WebView2 宿主模块
4. 调整主流程和主循环
5. 删除旧的 Edge 启动逻辑
6. 做完整手工验证

## 第一步：准备开发环境

需要安装或确认以下组件：

- Visual Studio 2022 或 Build Tools
- Desktop development with C++
- MSVC 编译工具链
- Windows SDK
- CMake 工具
- WebView2 SDK
- 已安装 WebView2 Runtime

当前已确认：

- 本机已有 WebView2 Runtime
- 当前环境未发现 WebView2 SDK 头文件和库
- 当前项目使用 MinGW，不适合直接平滑接入 WebView2

## 第二步：先迁移编译链到 MSVC

目标：

- 不改业务逻辑
- 不接 WebView2
- 先让当前项目在 MSVC 下编译通过

需要调整：

- `CMakeLists.txt`
- `build.bat`
- `README.md`

建议改造后支持的构建方式：

```powershell
cmake -S . -B build-msvc -G "Visual Studio 17 2022"
cmake --build build-msvc --config Release
```

这一阶段只解决：

- 编译器切换
- 构建目录切换
- 现有 Win32/Winsock 代码在 MSVC 下可用

## 第三步：新增 WebView2 宿主模块

建议新增文件：

- `src/webview_host.h`
- `src/webview_host.cpp`

这个模块负责：

- 注册主窗口类
- 创建 Win32 主窗口
- 初始化 COM
- 创建 WebView2 Environment
- 创建 WebView2 Controller
- 创建并保存 WebView 实例
- 导航到 `http://127.0.0.1:5188/`
- 在 `WM_SIZE` 中同步调整 WebView 区域
- 在关闭窗口时触发程序退出

建议接口保持简单，例如：

```cpp
bool start_webview(AppContext& app);
void stop_webview(AppContext& app);
bool webview_running(const AppContext& app);
```

## 第四步：扩展应用状态

需要调整：

- `src/app_types.h`

当前 `AppContext` 里主要是：

- overlay 状态
- socket 状态
- Edge 进程句柄

迁移后建议改成保存：

- `HWND main_wnd`
- WebView2 Environment 指针
- WebView2 Controller 指针
- WebView2 WebView 指针
- WebView 窗口就绪状态

旧的这些字段后续可删除：

- `PROCESS_INFORMATION ui_proc`
- `launch_error`
- `app_mode`

## 第五步：调整主程序流程

需要调整：

- `src/main.cpp`

当前流程是：

1. 启动 overlay
2. 启动 HTTP server
3. 启动 Edge
4. 主线程自己跑 accept 循环

迁移后建议变成：

1. 启动 overlay
2. 启动 HTTP server
3. 创建 WebView2 主窗口
4. 主线程进入 Win32 消息循环
5. 窗口关闭时统一退出 server 和 overlay

关键决策：

- 以后由窗口消息循环充当主循环
- HTTP server 改为后台线程运行

## 第六步：改造 HTTP server 运行方式

需要调整：

- `src/server.h`
- `src/server.cpp`

当前问题：

- server 逻辑由主线程 accept 循环驱动
- 不适合与 WebView2 主窗口消息循环并存

建议改成：

- `start_server(app)` 负责初始化监听 socket
- 新增后台线程循环 accept + handle
- `stop_server(app)` 负责结束线程并关闭 socket

要求：

- 行为和当前 API 保持一致
- 保留静态资源服务逻辑
- 保留 `/api/state`、`/api/apply`、`/api/profile/*`、`/api/quit`

## 第七步：删除旧的 Edge 启动逻辑

需要调整：

- `src/ui_launcher.h`
- `src/ui_launcher.cpp`

当前这些能力后续不再需要：

- 查找 `msedge.exe`
- `CreateProcessW` 启动 `--app=...`
- `stop_ui()` 终止 Edge 进程

建议处理方式：

- 删除 `ui_launcher.*`
- 由 `webview_host.*` 完全替代

如果想平滑迁移，也可以先保留文件但停止调用，最后再删。

## 第八步：CMake 接入 WebView2

需要调整：

- `CMakeLists.txt`

要做的事情：

- 把 `src/webview_host.cpp` 加进目标
- 配置 WebView2 SDK include 路径
- 链接 `WebView2Loader`
- 保留现有 `ws2_32` 链接

注意：

- 不要把 SDK 路径硬编码成个人电脑绝对路径作为长期方案
- 最好通过变量、环境变量或 README 文档说明来配置

## 验收清单

### 编译验证

- MSVC 下能够成功编译
- Release 构建成功生成可执行文件

### 窗口验证

- 启动后主窗口尺寸固定且可控
- 可以正确响应最小化、关闭、调整大小

### WebView 验证

- 能加载 `http://127.0.0.1:5188/`
- 前端页面样式和脚本都正常生效

### 功能验证

- `GET /api/state`
- `POST /api/apply`
- `POST /api/profile/load`
- `POST /api/profile/save`
- `POST /api/profile/new`
- `POST /api/profile/rename`
- `POST /api/quit`

### 生命周期验证

- 关闭主窗口后程序完整退出
- overlay 正常停止
- server 正常停止

## 推荐提交顺序

建议拆成 3 个提交：

1. `refactor: migrate build flow to MSVC`
2. `feat: embed web ui with webview2 host`
3. `refactor: remove external edge app launcher`

## 涉及文件总览

预计涉及文件：

- `CMakeLists.txt`
- `build.bat`
- `README.md`
- `src/app_types.h`
- `src/main.cpp`
- `src/server.h`
- `src/server.cpp`
- `src/ui_launcher.h`
- `src/ui_launcher.cpp`
- `src/webview_host.h`
- `src/webview_host.cpp`

## 结论

这条路线可行，但不建议直接跳到 WebView2 实现。

正确顺序应当是：

1. 先补齐 MSVC + WebView2 SDK 环境
2. 再把项目构建链切到 MSVC
3. 再增加 WebView2 宿主模块
4. 最后删掉外部 Edge 启动方式

这样风险最低，出问题也容易定位。
