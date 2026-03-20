# MyCross

<p align="center">
  <b>Web UI 驱动的 Windows 准星叠加工具</b>
</p>

## 这次重构

- 前端改为 `Web UI`（本地浏览器界面）
- 后端改为内置 `HTTP API`（`127.0.0.1:5188`）
- 准星渲染仍由 Win32 完成（置顶、透明、鼠标穿透）
- 支持多配置文件的加载/新建/保存/重命名

## 启动方式

```bat
build.bat
crosshair.exe
```

程序启动后会自动打开“应用窗口模式”（无地址栏、无标签页，外观类似普通桌面软件）：

```text
http://127.0.0.1:5188/
```

## Web UI 功能

- 准星开关（启动/停止）
- 参数实时应用（坐标、尺寸、线宽、RGB）
- 配置管理：
  - 加载配置
  - 新建配置
  - 保存当前配置
  - 重命名配置
- 颜色实时预览

## 全局热键

- 关闭准星：`Ctrl + Alt + Shift + F12`

## 配置目录

```text
configs/*.ini
```

默认配置：

```text
configs/default.ini
```

## 主要文件

```text
crosshair.cpp     # Win32 渲染 + HTTP API 服务
web/index.html    # Web UI 前端
build.bat         # 构建脚本
```

## 退出程序

- 在 Web UI 中点击“退出程序”按钮

## 说明

- 默认使用 Edge `--app` 方式承载 Web UI（避免普通浏览器标签页体验）
- 若应用窗口启动失败，会自动回退到默认浏览器打开
