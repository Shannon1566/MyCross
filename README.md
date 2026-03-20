# MyCross

<p align="center">
  <b>轻量、透明、置顶的 Windows 准星叠加工具</b>
</p>

<p align="center">
  <img alt="platform" src="https://img.shields.io/badge/Platform-Windows-0078D6?style=for-the-badge&logo=windows" />
  <img alt="language" src="https://img.shields.io/badge/Language-C%2B%2B17-00599C?style=for-the-badge&logo=cplusplus" />
  <img alt="build" src="https://img.shields.io/badge/Build-g%2B%2B-brightgreen?style=for-the-badge" />
  <img alt="status" src="https://img.shields.io/badge/Status-Usable-success?style=for-the-badge" />
</p>

---

## 项目简介

`MyCross` 是一个面向 Windows 的小工具：在屏幕上绘制一个始终置顶、可穿透点击的准星层。  
适用于 FPS 训练、瞄准习惯校准、无准星场景辅助等用途。

## 亮点功能

- 始终置顶显示，不易被其他窗口遮挡
- 鼠标可穿透，不影响点击和拖拽操作
- 支持坐标启动，便于固定到特定位置
- 支持 `Esc` 一键退出，使用成本低
- 单文件源码，易读、易改、易二次开发

## 效果说明

程序会创建一个透明弹窗，仅绘制绿色十字线条，黑色背景被透明色键抠除。

## 快速开始

### 1. 克隆项目

```bat
git clone https://github.com/Shannon1566/MyCross.git
cd MyCross
```

### 2. 编译

方式 A：脚本编译

```bat
build.bat
```

方式 B：手动编译

```bat
g++ -std=c++17 -O2 -municode -mwindows crosshair.cpp -o crosshair.exe
```

### 3. 运行

默认在屏幕中心显示：

```bat
crosshair.exe
```

按绝对坐标显示（位置更可控）：

```bat
crosshair.exe 960 540
```

或使用命名参数：

```bat
crosshair.exe --x=960 --y=540
```

查看帮助：

```bat
crosshair.exe --help
```

退出程序：`Esc`

## 参数说明

| 参数形式 | 说明 | 示例 |
|---|---|---|
| 无参数 | 使用屏幕中心点 | `crosshair.exe` |
| 位置参数 | 第 1 个为 `x`，第 2 个为 `y` | `crosshair.exe 800 450` |
| 命名参数 | 使用 `--x=` 与 `--y=` | `crosshair.exe --x=800 --y=450` |
| 帮助参数 | 显示使用帮助 | `crosshair.exe --help` |

## 技术实现（简要）

- Win32 API 创建 `WS_POPUP` 无边框窗口
- 扩展样式：`WS_EX_TOPMOST`、`WS_EX_LAYERED`、`WS_EX_TRANSPARENT`、`WS_EX_NOACTIVATE`
- `SetLayeredWindowAttributes` 做颜色键透明
- 在 `WM_PAINT` 中用 GDI 绘制十字准星
- 注册热键 `Esc`（`RegisterHotKey`）实现快速退出

## 项目结构

```text
MyCross/
├─ crosshair.cpp   # 主程序源码
├─ build.bat       # 一键构建脚本
├─ .gitignore
└─ README.md
```

## 开发建议

可继续扩展的方向：

- 支持准星颜色/大小/线宽参数化
- 支持配置文件（INI/JSON）
- 多准星样式（圆点、圆环、动态扩散）
- 开机自启与托盘菜单

## 免责声明

本项目仅用于学习与个人辅助用途，请遵守目标软件/平台的使用协议与当地法规。

---

<p align="center">
  如果这个项目对你有帮助，欢迎 Star。
</p>
