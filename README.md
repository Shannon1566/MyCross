# MyCross

<p align="center">
  <b>一个基于 C++、Win32 Overlay 与 Qt/QML 的游戏准星叠加工具</b><br>
  <sub>Lightweight Crosshair Overlay Tool built with C++ and Qt Quick</sub>
</p>

---

## 功能特性

- 启动 / 停止准星叠加
- 参数调节：位置、尺寸、线宽、RGB 颜色
- 配置管理：新建、加载、保存、重命名
- 下拉选择配置时自动应用
- 全局快捷键关闭准星：`Ctrl + Alt + Shift + F12`

---

## 技术实现

- 使用 C++17 管理应用状态、配置文件和 Win32 准星覆盖层
- 使用 Qt 6 + QML 构建控制台界面
- QML 通过 `CrosshairController` 直接调用 C++ 后端对象
- 准星覆盖层继续使用独立 Win32 线程绘制，保留透明、置顶、鼠标穿透和全局热键行为
- 配置继续保存为 `configs/*.ini`

---

## 构建与运行

### 环境要求

- Windows 11
- Visual Studio 2022 Build Tools，安装 `Desktop development with C++`
- CMake
- Qt 6.11.0 MSVC 2022 64-bit kit，默认路径：`D:\Qt\6.11.0\msvc2022_64`

