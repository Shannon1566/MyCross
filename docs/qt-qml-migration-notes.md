# Qt/QML 迁移说明

MyCross 已从 WebView2 前端迁移到 Qt 6 + QML。

## 当前架构

- `src/main.cpp` 启动 Qt 应用、初始化配置、启动 Win32 overlay 线程并加载 QML。
- `src/crosshair_controller.*` 是 QML 与 C++ 后端之间的唯一公开桥接层。
- `src/overlay.*` 保留准星覆盖层线程、透明窗口绘制和全局热键。
- `src/config_store.*` 继续负责 `configs/*.ini` 的读写和字段归一化。
- `qml/Main.qml` 是控制台主界面。

## 已移除内容

- WebView2 宿主窗口
- `frontend/` HTML/CSS/JS 前端
- 旧本地 HTTP server 源码
- WebView2 include/link 构建依赖

## 构建

默认 Qt kit：

```text
D:\Qt\6.11.0\msvc2022_64
```

构建命令：

```bat
build.bat
```

构建输出：

```text
build-qt-vs\Release\crosshair.exe
```

## 验证重点

- QML 控制台能启动
- 准星启动、停止、参数调整即时生效
- Profile 新建、加载、保存、重命名正常
- 全局热键 `Ctrl + Alt + Shift + F12` 能关闭准星
- 程序退出时 overlay 线程被正确回收
