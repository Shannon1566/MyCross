# MyCross

重构版 `MyCross`：

- C++ 后端：准星渲染 + 本地 HTTP API + 配置文件管理
- Web 前端：现代化控制台（自动加载配置、新建、保存、重命名、启动/停止）
- 启动后默认打开应用窗口模式（Edge `--app`），失败时回退默认浏览器

## 运行

```bat
build.bat
crosshair.exe
```

## 功能

- 启动/停止准星叠加
- 参数调整：坐标、尺寸、线宽、RGB
- 配置管理：加载、新建、保存、重命名
- 下拉框选中后自动加载
- 全局热键关闭准星：`Ctrl + Alt + Shift + F12`

## API（本地）

- `GET /api/state`
- `POST /api/ping`
- `POST /api/toggle`
- `POST /api/apply`
- `POST /api/profile/load`
- `POST /api/profile/save`
- `POST /api/profile/new`
- `POST /api/profile/rename`
- `POST /api/quit`

## 目录

- `crosshair.cpp` 后端与 Win32 渲染
- `web/index.html` 前端界面
- `configs/*.ini` 配置文件
