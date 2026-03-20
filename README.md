# MyCross

<p align="center">
  <b>Windows 准星叠加工具（图形化控制台 + 多配置文件）</b>
</p>

<p align="center">
  <img alt="platform" src="https://img.shields.io/badge/Platform-Windows-0078D6?style=for-the-badge&logo=windows" />
  <img alt="language" src="https://img.shields.io/badge/Language-C%2B%2B17-00599C?style=for-the-badge&logo=cplusplus" />
  <img alt="ui" src="https://img.shields.io/badge/UI-Win32-success?style=for-the-badge" />
  <img alt="profiles" src="https://img.shields.io/badge/Profiles-INI-informational?style=for-the-badge" />
</p>

---

## 项目简介

`MyCross` 是一个轻量级 Windows 准星工具。双击程序后会进入图形化控制台，你可以在 UI 中：

- 打开 / 关闭准星
- 调整坐标、尺寸、线宽、颜色
- 选择配置文件
- 新建并保存多套配置

## 核心功能

- 图形化控制窗口（无需命令行也可完整使用）
- 准星窗口置顶、透明、鼠标穿透
- 低冲突关闭热键：`Ctrl + Alt + Shift + F12`
- 配置文件持久化（`configs/*.ini`）
- 支持多配置切换与快速新建

## 快速开始

### 1. 编译

```bat
build.bat
```

或手动：

```bat
g++ -std=c++17 -O2 -municode -mwindows crosshair.cpp -o crosshair.exe
```

### 2. 运行

```bat
crosshair.exe
```

双击 `crosshair.exe` 也是同样效果：会打开 `MyCross 控制台`。

## 图形化使用流程

1. 在顶部下拉框选择配置文件（默认 `default.ini`）
2. 修改参数：`X/Y`、`窗口尺寸`、`准星半径`、`线宽`、`颜色 RGB`
3. 点击 `打开准星` 开启叠加
4. 点击 `保存` 将当前参数写入当前配置文件
5. 点击 `新建` 基于当前参数生成新配置（如 `profile_001.ini`）
6. 任意时刻可用 `Ctrl + Alt + Shift + F12` 关闭准星

## 参数说明

| 参数 | 说明 |
|---|---|
| X / Y | 准星中心屏幕坐标，`-1` 表示自动居中 |
| 窗口尺寸 | 准星窗口总大小（像素） |
| 准星半径 | 十字线向外延伸长度 |
| 线宽 | 线条粗细 |
| 颜色 R/G/B | 准星颜色通道值（0-255） |

## 配置文件

配置目录：

```text
configs/
```

每个配置都是一个 `ini` 文件，例如：

```ini
[Crosshair]
x=-1
y=-1
window_size=40
cross_half=10
line_width=2
color_r=0
color_g=255
color_b=0
```

## 项目结构

```text
MyCross/
├─ crosshair.cpp
├─ build.bat
├─ .gitignore
├─ README.md
└─ configs/            # 运行后自动生成
```

## 免责声明

本项目仅用于学习与个人辅助用途，请遵守目标软件/平台的使用协议与当地法规。
