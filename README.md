# MyCross

重构版 `MyCross`：

- C++ 后端：准星渲染 + 配置文件管理 + 原生消息桥
- Web 前端：现代化控制台（自动加载配置、新建、保存、重命名、启动/停止）
- 启动后以内嵌 WebView2 窗口承载前端控制台

## 运行

```bat
build.bat
build-msvc\crosshair.exe
```

说明：默认使用 CMake + Visual Studio 2022（MSVC，x64）构建。

## 功能

- 启动/停止准星叠加
- 参数调整：坐标、尺寸、线宽、RGB
- 配置管理：加载、新建、保存、重命名
- 下拉框选中后自动加载
- 全局热键关闭准星：`Ctrl + Alt + Shift + F12`

## 通信

- 前端与 C++ 后端通过 WebView2 `postMessage` 桥接通信
- 控制方法覆盖原有状态读取、参数应用、配置管理与退出流程

## 目录

- `frontend/index.html` 前端源码
- `configs/*.ini` 配置文件
- `build-msvc/` CMake 构建输出（运行时资源会自动拷贝到 `build-msvc/configs` 与 `build-msvc/web`）
