# MyCross C++ 后端运行过程说明

## 总览

当前 MyCross 由三部分组成：

- Qt/QML 控制台：`qml/Main.qml`
- C++ 控制器：`src/crosshair_controller.*`
- Win32 准星覆盖层：`src/overlay.*`

程序不再使用 WebView2，也不再启动本地 HTTP 服务。QML 通过 `CrosshairController` 直接调用 C++ 后端。

## 启动流程

1. `src/main.cpp` 创建 `QGuiApplication`。
2. 初始化 `AppContext`，设置 `exe_dir` 与 `cfg_dir`。
3. `ensure_cfg()` 确保 `configs` 目录和默认配置存在。
4. 读取 `default.ini` 到内存状态。
5. 应用命令行中的 `x/y` 覆盖值。
6. `start_overlay()` 启动准星线程。
7. 创建 `CrosshairController` 并注册为 QML 上下文属性 `crosshair`。
8. 加载 `qml/Main.qml`。
9. Qt 事件循环运行，直到窗口关闭或用户点击退出。

## 状态模型

核心状态在 `AppContext::state`：

- `cfg`：当前准星参数。
- `running`：准星是否显示。
- `active`：当前激活的 profile 文件名。
- `mu`：保护共享状态的互斥锁。

QML 不直接访问 `AppContext`。它只读写 `CrosshairController` 暴露的属性和方法。

## QML 到 C++ 的调用

`CrosshairController` 暴露：

- `running`
- `activeProfile`
- `profiles`
- `config`
- `hotkey`
- `lastError`

主要方法：

- `setRunning(bool)`
- `applyConfig(map)`
- `loadProfile(name)`
- `saveProfile(name, map)`
- `createProfile(name, map)`
- `renameProfile(oldName, newName)`
- `quitApp()`

所有配置值最终都会在 C++ 侧经过 `normalize()`，QML 侧输入限制只负责交互体验。

## 准星刷新链路

用户在 QML 中修改参数后：

1. QML 调用 `crosshair.applyConfig(...)`。
2. `CrosshairController` 加锁更新 `app.state.cfg`。
3. 调用 `post_sync(app)`。
4. overlay 控制窗口收到 `WM_APP_SYNC`。
5. `apply_overlay(app)` 创建、移动、销毁或重绘透明准星窗口。

启动和停止准星也走同一条同步链路。

## 热键

全局热键仍由 overlay 线程注册：

```text
Ctrl + Alt + Shift + F12
```

按下热键后，overlay 线程会把 `running` 设置为 `false` 并立即关闭准星。`CrosshairController` 使用轻量定时同步，让 QML 状态跟随后端变化。

## 配置文件

配置继续使用 `configs/*.ini`：

- `profile_name()` 清洗文件名并补齐 `.ini`。
- `load_cfg()` 从 INI 读取配置。
- `save_cfg()` 保存配置。
- `profiles()` 枚举所有 profile。

迁移到 QML 不改变配置文件格式。
