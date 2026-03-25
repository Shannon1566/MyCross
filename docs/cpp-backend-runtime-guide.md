# MyCross C++ 后端运行过程说明

这份文档是给“完全没学过编程的人”看的。

你可以把它理解成一份“看图说话版”的后端说明：程序从哪里开始，先做什么，后做什么，用户点一下按钮以后，屏幕上的准星为什么会变。

---

## 先说结论

这个项目当前真正运行的 C++ 后端，不是一个单独的 `src/crosshair.cpp` 文件。

现在它已经拆成了几块：

- `src/main.cpp`
  程序总入口。程序一启动，先从这里开始。
- `src/app_types.h`
  放全局状态结构。可以把它理解成“程序的大总账本”。
- `src/common.cpp`
  放一些公共小工具，比如路径处理、字符串转换、数字限制。
- `src/config_store.cpp`
  负责读写 `configs/*.ini` 配置文件。
- `src/overlay.cpp`
  负责准星覆盖层，也就是屏幕上那个真正画出来的十字。
- `src/ui_launcher.cpp`
  负责创建主窗口、初始化 WebView2、接收前端页面发来的消息。

这个项目以前还有一套 `src/server.cpp` 的本地 HTTP 服务方案。

那套旧代码现在已经从当前实现里剔除了，所以它 **不属于现在真实运行的主流程**。

所以，要理解现在这个程序，先抓住一条主线：

1. `main.cpp` 启动程序
2. 读配置
3. 启动准星线程
4. 打开 WebView2 控制面板
5. 前端页面通过消息桥告诉 C++ 要做什么
6. C++ 修改状态
7. `overlay.cpp` 按最新状态把准星画到屏幕上
8. 用户退出时统一清理资源

---

## 当前程序到底是什么架构

你可以把整个程序想成 3 个部分一起合作：

- 前端页面
  就是 `frontend/index.html`、`frontend/app.js`、`frontend/styles.css` 这些网页文件。
- C++ 后端
  就是负责保存状态、处理命令、控制准星、管理窗口的代码。
- 准星覆盖层窗口
  这是一个单独的透明窗口，真正显示在屏幕上。

这里最容易搞混的一点是：

**现在的 UI 不是外部浏览器。**

当前版本里，`src/ui_launcher.cpp` 会自己创建一个 Win32 窗口，然后把 WebView2 嵌进去。也就是说，网页界面是“装在程序自己的窗口里”的，不是另外打开 Edge 浏览器。

---

## 先看构建结果

`CMakeLists.txt` 里当前参与构建的源文件是：

- `src/main.cpp`
- `src/app_types.h`
- `src/common.h`
- `src/common.cpp`
- `src/config_store.h`
- `src/config_store.cpp`
- `src/overlay.h`
- `src/overlay.cpp`
- `src/ui_launcher.h`
- `src/ui_launcher.cpp`

这件事非常重要，因为它直接告诉我们：

- 当前真实入口是 `wWinMain`
- 当前真实 UI 是 WebView2 宿主窗口
- 当前前后端通信方式是 `postMessage`
- 当前不是“HTTP 服务 + 浏览器页面”那条旧路线

---

## 程序从哪里开始

程序入口在 `src/main.cpp` 的 `wWinMain`。

这个函数可以理解成：“Windows 程序一启动，系统先敲这个门。”

`wWinMain` 做的事情，顺序非常清楚：

1. 创建 `AppContext`
2. 计算程序目录、配置目录、前端目录
3. 确保配置文件存在
4. 把默认配置读进内存
5. 读取命令行参数
6. 启动准星线程
7. 启动 WebView2 主窗口
8. 进入主消息循环
9. 收到退出信号后，统一清理 UI 和准星线程

如果你只想先抓大框架，记住这 9 步就够了。

---

## `AppContext` 是什么

`AppContext` 定义在 `src/app_types.h`。

它可以理解成：

**程序运行时最重要的总箱子。**

很多地方都要用的数据，都会塞进这个箱子里。

它里面最重要的内容有：

- `state`
  当前配置和当前运行状态。
- `exit`
  退出开关。它是一个原子变量，你可以把它理解成“全局安全退出标志”。
- `inst`
  Windows 程序实例句柄。很多创建窗口的操作都要用它。
- `exe_dir`
  当前程序所在目录。
- `cfg_dir`
  配置目录，也就是 `configs`。
- `web_dir`
  前端页面目录，也就是 `web`。
- `overlay_thread`
  负责准星覆盖层的线程。
- `overlay_ready`
  准星线程有没有准备好。
- `ctl_wnd`
  准星控制窗口句柄。
- `overlay_wnd`
  真正画准星的透明窗口句柄。
- `ui_wnd`
  主界面窗口句柄。
- `launch_error`
  启动 UI 失败时记录错误码。

这就像一辆车的“总仪表盘”。

程序里不同模块虽然各做各的事，但都围着这个 `AppContext` 转。

---

## `State` 和 `Config` 是什么

这两个也在 `src/app_types.h`。

### `Config`

`Config` 里放的是准星参数：

- `x`
- `y`
- `window_size`
- `cross_half`
- `line_width`
- `color_r`
- `color_g`
- `color_b`

也就是说，准星画在什么位置、多大、多粗、什么颜色，都在这里。

### `State`

`State` 里有：

- `cfg`
  当前正在使用的那份准星配置。
- `running`
  准星现在是不是显示中。
- `active`
  当前激活的是哪个配置文件，比如 `default.ini`。
- `mu`
  互斥锁。

这里的锁不用想得太复杂。

它的作用你可以直接理解成一句话：

**防止多个地方同时改同一份数据，把数据改乱。**

比如：

- 前端刚让程序改颜色
- 准星线程也刚好在读颜色准备画图

这两个动作如果毫无保护，很容易撞车。

所以程序在读写 `app.state` 的关键地方，都会先加锁。

---

## 第一步：程序先准备路径

`wWinMain` 刚开始会创建 `AppContext app;`，然后设置几个目录：

- `app.exe_dir = exe_dir()`
- `app.cfg_dir = app.exe_dir + "\\configs"`
- `app.web_dir = app.exe_dir + "\\web"`

这里用到的 `exe_dir()` 在 `src/common.cpp`。

它做的事很朴素：

1. 先问 Windows：“当前这个 exe 文件在哪个目录？”
2. 再把文件名那一段去掉
3. 只保留目录路径

这样程序就知道自己在磁盘上的位置了。

后面很多动作都靠它：

- 去哪里找配置文件
- 去哪里找网页文件

---

## 第二步：确保配置目录和默认配置存在

这一步在 `src/config_store.cpp` 的 `ensure_cfg(app)`。

它做两件事：

1. 创建 `configs` 目录
2. 如果里面一个 `.ini` 都没有，就自动生成一个 `default.ini`

你可以把它理解成：

“先把抽屉准备好。如果抽屉里还是空的，就先放一份默认说明书进去。”

这样程序后面再去读配置时，就不容易扑空。

---

## 第三步：把默认配置读进内存

在 `main.cpp` 里，程序会先把当前激活配置名设成 `default.ini`，然后调用：

- `load_cfg(profile_path(app, app.state.active))`

这里涉及 `src/config_store.cpp` 里的几个函数：

- `profile_path`
  把配置名拼成完整路径。
- `load_cfg`
  从 `.ini` 文件读取配置值。
- `normalize`
  把不合理的值修正到合法范围内。

### `load_cfg` 做了什么

它会从 INI 文件的 `Crosshair` 段读取这些字段：

- `x`
- `y`
- `window_size`
- `cross_half`
- `line_width`
- `color_r`
- `color_g`
- `color_b`

如果文件里缺某个字段，就保留结构体里的默认值。

### `normalize` 做了什么

它负责“兜底修正”。

比如：

- `window_size` 不能太小，也不能大得离谱
- `line_width` 不能小于 1
- 颜色必须在 0 到 255 之间
- `x` 和 `y` 小于 `-1` 时会被修正回 `-1`

其中 `-1` 在这里的意思是：

**居中。**

所以如果配置里写 `x=-1`、`y=-1`，程序后面会把准星放在屏幕中心。

---

## 第四步：处理命令行参数

这一步在 `src/ui_launcher.cpp` 里的 `apply_cli(app)`。

虽然它放在 `ui_launcher.cpp`，但它其实是在程序启动早期就被 `main.cpp` 调用的。

它支持两种方式给位置参数：

1. 直接给两个位置值
   比如程序后面跟两个数字
2. 用 `--x=` 和 `--y=` 这种形式

它会把这些命令行参数覆盖到当前内存里的 `Config` 上，然后再做一次 `normalize(cfg)`。

也就是说，命令行参数的优先级，会高于刚从 `default.ini` 里读出来的值。

---

## 第五步：启动准星线程

这一步在 `src/overlay.cpp` 的 `start_overlay(app)`。

线程这个词如果你不熟，可以简单理解成：

**程序里一条单独干活的线。**

这个项目里，准星显示被放进一条单独的线程里做。

原因很简单：

- 主线程要管主窗口和 WebView2
- 准星那边也要自己收消息、管窗口、重绘

所以作者把它们拆开了。

### `start_overlay` 做了什么

它会：

1. 启动一个新线程
2. 在线程里跑 `overlay_thread_main`
3. 稍微等一下，直到 `overlay_ready` 变成真

这就像：

- 主线程说：“你去负责准星那边。”
- 新线程说：“好，我先把我的办公室搭起来。”
- 办公室搭好了，就把 `overlay_ready` 设成真

---

## 第六步：准星线程自己建两种窗口

`overlay_thread_main` 是准星线程的真正入口。

它会注册两类窗口：

- `OV_CLASS`
  真正显示准星的窗口
- `CTL_CLASS`
  控制窗口

这里的控制窗口不是给用户看的。

它更像程序内部的“值班室”，专门用来接消息。

### 为什么要有控制窗口

因为准星线程需要处理几类事情：

- 收到“同步状态”的通知
- 收到热键消息
- 收到退出消息

这些都可以通过窗口消息机制来处理。

所以程序创建了一个隐藏的控制窗口 `ctl_wnd`，专门负责收这种内部消息。

### 热键也是在这里注册的

`overlay_thread_main` 里会调用：

- `RegisterHotKey(app->ctl_wnd, HOTKEY_ID, HOTKEY_MOD, HOTKEY_VK);`

对应的快捷键就是：

`Ctrl + Alt + Shift + F12`

这表示不管前台是什么窗口，只要这个程序还活着，这个热键就能把准星关掉。

---

## 第七步：先发一次同步消息

`main.cpp` 在启动完准星线程后，会调用一次：

- `post_sync(app)`

这个函数也在 `src/overlay.cpp`。

它做的事情非常简单：

- 如果控制窗口 `ctl_wnd` 存在
- 就给它发一个 `WM_APP_SYNC` 消息

你可以把 `post_sync` 理解成一句口头通知：

**“喂，准星线程，你现在按最新状态刷新一下。”**

这个函数在整个项目里很关键。

后面这些操作完成后，都会再调它一次：

- 启动时初次刷新
- 前端修改准星参数
- 前端切换显示状态
- 加载配置
- 保存配置
- 新建配置

因为“状态改了”不等于“屏幕自动变了”。

真正让屏幕变化的，是把同步消息送到准星线程那里。

---

## 第八步：启动主界面窗口

这一步在 `src/ui_launcher.cpp` 的 `launch_ui(app)`。

它负责做的事很多，但主线并不乱。

### 8.1 先初始化 COM

它先调用 `CoInitializeEx`。

这个东西你可以先不用深究原理。

在这份文档里，你只要把它理解成：

**WebView2 开工前，需要先把底层环境准备好。**

如果这一步失败，程序就没法正常创建 WebView2。

### 8.2 注册主窗口类

接着它会注册一个窗口类 `UI_CLASS`，窗口过程是 `UiProc`。

这个主窗口就是你看到的控制面板外壳。

### 8.3 创建主窗口

然后调用 `CreateWindowExW` 创建 `app.ui_wnd`。

窗口标题是 `MyCross`，大小大约是 `1120 x 700`。

### 8.4 显示窗口

创建完以后会：

- `ShowWindow`
- `UpdateWindow`

这就是让窗口真正显示出来。

### 8.5 初始化 WebView2

最后调用 `init_webview(app)`。

如果这一步失败，窗口会被销毁，程序启动也算失败。

---

## 第九步：把前端网页装进 WebView2

`init_webview(app)` 是 UI 这边最关键的一段。

它做的事可以拆成几步：

1. 创建 WebView2 环境
2. 创建 WebView2 控制器
3. 拿到真正的 WebView 对象
4. 让 WebView 填满宿主窗口客户区
5. 把本地 `web` 目录映射成一个虚拟域名
6. 注册“网页消息接收回调”
7. 导航到 `https://app.mycross.local/index.html`

### 虚拟域名映射是什么意思

代码里用了：

- `SetVirtualHostNameToFolderMapping`

它把本地目录 `app.web_dir` 映射到：

- `app.mycross.local`

所以后面 WebView2 打开：

- `https://app.mycross.local/index.html`

其实访问的是程序目录里的网页文件。

这就像给本地文件夹起了一个“站点名字”，让 WebView2 按网页方式去加载它们。

---

## 第十步：前端页面是怎么和 C++ 说话的

这里要同时看两边：

- 前端：`frontend/app.js`
- 后端：`src/ui_launcher.cpp`

### 前端怎么发消息

前端 JS 里有一个 `bridge.invoke(method, params)`。

它会：

1. 生成一个请求编号 `id`
2. 把方法名和参数打包成 `URLSearchParams`
3. 调用 `window.chrome.webview.postMessage(...)`

也就是说，前端不是发 HTTP 请求，而是直接把一串消息交给 WebView2。

### 后端怎么收消息

`ui_launcher.cpp` 里给 WebView 注册了 `add_WebMessageReceived(...)` 回调。

网页一发消息，这个回调就会被触发。

回调里会做几件事：

1. 取出消息字符串
2. 转成 UTF-8 字符串
3. 用 `form_parse` 解析成键值对
4. 交给 `handle_bridge_call(*g_app, form)` 处理
5. 把处理结果再发回前端

所以整条路就是：

前端按钮 -> `postMessage` -> C++ 回调 -> 改状态 -> 回消息给前端

---

## 第十一步：C++ 收到前端消息后做什么

核心处理函数是 `src/ui_launcher.cpp` 里的 `handle_bridge_call`。

它像一个分发台。

前端说不同的话，它就走不同分支。

当前支持的主要方法有：

- `state.get`
  取当前状态
- `overlay.set_running`
  设置准星开关
- `config.apply`
  应用参数
- `profile.load`
  加载配置文件
- `profile.save`
  保存当前配置
- `profile.create`
  新建配置
- `profile.rename`
  重命名配置
- `app.quit`
  退出程序

这些方法名，前端 `app.js` 里也能对上。

### 它的共同行为

不管是哪一种操作，基本都绕不开这几件事：

1. 先从消息里取参数
2. 必要时加锁访问 `app.state`
3. 更新内存中的状态
4. 如果需要，就读写磁盘上的 `.ini`
5. 如果状态会影响准星显示，就调用 `post_sync(app)`
6. 拼一个 JSON 结果返回给前端

这就是整个后端的工作模式：

**先改内存状态，再通知准星线程刷新，再把新状态回给页面。**

---

## 第十二步：当前状态怎么返回给前端

这个工作主要由 `state_json(app)` 完成。

它会读取当前状态，组装成一段 JSON。

里面主要包括：

- `running`
- `active_profile`
- `hotkey`
- `config`
- `profiles`

也就是：

- 准星现在开没开
- 当前用的是哪个配置
- 热键是什么
- 当前参数是多少
- 配置列表有哪些

前端收到这段 JSON 后，就能刷新界面：

- 下拉框显示哪个配置
- 输入框显示哪些参数
- 状态灯亮不亮
- 按钮文字是“启动准星”还是“停止准星”

---

## 第十三步：配置文件是怎么处理的

配置相关工作在 `src/config_store.cpp`。

这部分后端做的事很像“档案管理员”。

### `profiles(app)`

扫描 `configs` 目录，把所有 `.ini` 文件列出来，再排序。

### `profile_name(name)`

把用户输入的配置名清洗一下。

比如会处理这些问题：

- 去掉首尾空白
- 把 Windows 文件名不允许的字符替换成下划线
- 如果没写 `.ini`，就自动补上

这是为了避免把配置保存成非法文件名。

### `load_cfg(file)`

从 INI 文件读取参数。

### `save_cfg(file, cfg)`

把当前参数写回 INI 文件。

保存前还会先 `normalize(cfg)`，防止脏数据落盘。

### `ensure_cfg(app)`

保证配置目录和默认配置存在。

---

## 第十四步：准星是怎么真正画出来的

真正画准星的关键在 `src/overlay.cpp`。

### `apply_overlay(app)` 是真正的执行者

前面多次提到，状态变更后会调用 `post_sync(app)`。

而准星线程里的控制窗口收到 `WM_APP_SYNC` 后，会执行：

- `apply_overlay(*g_app);`

这一步非常关键。

因为它才是“把内存状态变成屏幕结果”的地方。

### `apply_overlay` 先看 `running`

它会先把当前 `cfg` 和 `running` 从 `app.state` 里拷出来。

然后分两种情况：

#### 情况 1：`running == false`

如果准星当前不该显示：

- 如果 `overlay_wnd` 存在，就销毁它
- 然后返回

也就是说，关闭准星本质上就是把那个透明准星窗口删掉。

#### 情况 2：`running == true`

如果准星应该显示：

1. 用 `overlay_rect(cfg)` 算出窗口矩形
2. 如果窗口还不存在，就创建一个新的透明顶层窗口
3. 如果窗口已存在，就移动窗口到新位置、新大小
4. 调用 `InvalidateRect`，要求重绘

### `overlay_rect(cfg)` 做了什么

它根据配置决定准星窗口放哪儿。

逻辑很直白：

- 如果 `x == -1`，就把横坐标放到屏幕中心
- 如果 `y == -1`，就把纵坐标放到屏幕中心
- 再根据 `window_size` 算出窗口的上下左右边界

这说明一件事：

程序不是直接在全屏上乱画。

它是创建了一个很小的透明窗口，把这个窗口摆到准星该出现的地方，然后只在这个小窗口里画十字。

---

## 第十五步：`WM_PAINT` 里到底画了什么

画图逻辑在 `OverlayProc` 的 `WM_PAINT` 分支里。

你可以把 `WM_PAINT` 理解成：

**Windows 在说：“这个窗口现在该重新画一遍了。”**

这时程序会：

1. 从 `app.state` 里拿当前配置
2. 创建一支画笔，颜色来自 `color_r/g/b`，粗细来自 `line_width`
3. 算出窗口中心点
4. 画一条横线
5. 画一条竖线

这样就得到一个十字准星。

### 为什么背景能透明

创建窗口时用了这些扩展样式：

- `WS_EX_TOPMOST`
  总在最上层
- `WS_EX_LAYERED`
  支持分层透明
- `WS_EX_TRANSPARENT`
  让鼠标事件尽量穿过去
- `WS_EX_NOACTIVATE`
  显示它时不抢焦点

然后又调用了：

- `SetLayeredWindowAttributes(..., RGB(0, 0, 0), 0, LWA_COLORKEY)`

这表示把黑色当成透明色。

所以结果就是：

- 黑背景不显示
- 只有绿色十字线露出来

---

## 第十六步：热键为什么能直接关准星

还是在 `src/overlay.cpp`。

控制窗口收到 `WM_HOTKEY` 且热键编号匹配时，会做这件事：

1. 加锁
2. 把 `app.state.running = false`
3. 直接调用 `apply_overlay(*g_app)`

这表示按下热键后，程序会立刻把“准星正在运行”改成“准星已关闭”，然后马上处理覆盖层。

所以效果就是：

**你一按热键，准星窗口就被销毁。**

这条路径甚至不需要前端参与。

也就是说，哪怕控制面板页面此时没有操作，这个热键照样有效。

---

## 第十七步：主线程为什么不会马上结束

`main.cpp` 里在 UI 启动成功后，会进入一个消息循环：

1. `GetMessageW`
2. `TranslateMessage`
3. `DispatchMessageW`

这就是所谓的“消息循环”。

如果你不熟这个概念，可以把它理解成：

**程序坐在那里等通知。谁发来消息，它就处理谁。**

为什么需要它？

因为 Windows 桌面程序不是“跑完一段代码就结束”的那种。

只要窗口还活着，程序就得一直等这些事情：

- 窗口大小变化
- 窗口关闭
- WebView2 初始化过程里的消息
- 各种系统窗口消息

所以主线程会一直待在这个循环里，直到退出条件满足。

---

## 第十八步：窗口关闭时发生什么

主窗口的窗口过程在 `UiProc`。

这里最重要的两个消息是：

### `WM_CLOSE`

当用户点右上角关闭按钮时，会：

1. 调用 `request_exit(*g_app)`
2. 再 `DestroyWindow(hwnd)`

### `request_exit(app)` 做了什么

它会：

1. 把 `app.exit = true`
2. 调用 `PostQuitMessage(0)`

这就相当于告诉主线程：

“可以准备收工了，主消息循环该退出了。”

### `WM_DESTROY`

窗口真正销毁后，会把 `app.ui_wnd` 清空，再发一次 `PostQuitMessage(0)`。

---

## 第十九步：程序退出时怎么收尾

`main.cpp` 的主消息循环一结束，就会按顺序调用：

1. `stop_ui(app)`
2. `stop_overlay(app)`

### `stop_ui(app)`

它会：

- 释放 WebView2 对象
- 销毁主窗口
- 如果 COM 是本程序初始化的，就做 `CoUninitialize`
- 清空相关全局指针和状态

### `stop_overlay(app)`

它会：

- 给控制窗口发 `WM_APP_EXIT`
- 准星线程收到后执行 `PostQuitMessage(0)`
- 准星线程自己的消息循环结束
- 最后主线程 `join` 这个线程

你可以把 `join` 理解成：

**主线程停下来等一下，直到准星线程真的收完工。**

这样程序退出就比较干净，不容易留下半死不活的后台资源。

---

## 线程和消息是怎么串起来的

这部分很重要。

如果你能理解这一节，整个程序的骨架就明白了。

### 1. 主线程负责什么

主线程主要负责：

- 程序入口
- 创建主窗口
- 初始化 WebView2
- 跑主消息循环
- 退出时统一清理

### 2. 准星线程负责什么

准星线程主要负责：

- 创建内部控制窗口
- 注册全局热键
- 创建和销毁准星透明窗口
- 处理同步消息
- 执行重绘

### 3. 前端页面负责什么

前端页面不直接画准星。

它只负责：

- 展示当前状态
- 收集用户输入
- 把“我要改什么”发给 C++

### 4. 真正的状态放在哪

真正的状态放在：

- `app.state`

这是后端的共享内存状态，不在网页里。

### 5. 真正触发准星变化的关键链路

最关键的一条链路是：

1. 前端发消息
2. C++ 改 `app.state`
3. C++ 调用 `post_sync(app)`
4. 控制窗口收到 `WM_APP_SYNC`
5. 调用 `apply_overlay(app)`
6. 创建、移动、销毁或重绘准星窗口

所以你可以把 `post_sync -> WM_APP_SYNC -> apply_overlay` 记成这个项目的核心刷新链。

---

## 从“点按钮”到“屏幕变化”的完整例子

下面用几个实际动作，把整条链路串起来。

### 例子 1：点击“启动准星”

前端 `app.js` 里，点击按钮后会：

1. 先取当前状态
2. 调用 `post("/api/toggle", { running: 1 })`
3. 这个路径在前端内部会被映射成方法 `overlay.set_running`

后端收到后：

1. `handle_bridge_call` 进入 `overlay.set_running`
2. 把 `app.state.running` 改成 `true`
3. 调用 `post_sync(app)`
4. 返回最新状态给前端

准星线程收到同步消息后：

1. 执行 `apply_overlay`
2. 发现 `running == true`
3. 创建透明准星窗口
4. 请求重绘

最后 `WM_PAINT` 画出十字。

### 例子 2：修改颜色

前端输入框失去焦点或按回车后，会触发自动应用：

1. 收集表单里的 `color_r/g/b`
2. 调用 `config.apply`

后端收到后：

1. 把表单值转成 `Config`
2. 执行 `normalize`
3. 更新 `app.state.cfg`
4. `post_sync(app)`

准星线程收到同步消息后：

1. `apply_overlay` 调整窗口并要求重绘
2. `WM_PAINT` 使用新的 RGB 颜色再画一遍

于是屏幕上的准星颜色就变了。

### 例子 3：加载一个 profile

前端在下拉框选择配置后，会调用 `profile.load`。

后端收到后：

1. 先把配置名清洗成合法文件名
2. 检查目标文件存在不存在
3. 用 `load_cfg` 读入配置
4. 更新 `app.state.cfg` 和 `app.state.active`
5. `post_sync(app)`
6. 把新状态回给前端

于是：

- 页面输入框更新了
- 准星也按新配置变了

### 例子 4：保存 profile

前端点击保存后，会调用 `profile.save`。

后端会：

1. 先决定保存到哪个配置名
2. 用表单值覆盖当前内存配置
3. 调用 `save_cfg(...)` 写入 `.ini`
4. 更新当前激活配置名
5. `post_sync(app)`
6. 返回最新状态

所以“保存”不是只改磁盘，也会顺手把当前显示状态一起同步。

### 例子 5：按热键关闭准星

这条路不经过前端。

1. 用户按下 `Ctrl + Alt + Shift + F12`
2. 控制窗口收到 `WM_HOTKEY`
3. 把 `running` 设为 `false`
4. 直接调用 `apply_overlay`
5. 准星窗口被销毁

所以热键关闭非常直接。

### 例子 6：点击退出或关闭窗口

1. 前端点“退出程序”，会发 `app.quit`
2. 或者用户直接点窗口右上角关闭按钮
3. 后端把 `app.exit` 设为真，并投递退出消息
4. 主消息循环结束
5. `stop_ui`
6. `stop_overlay`
7. 程序结束

---

## 当前代码里几个容易误会的点

### 1. 旧的 HTTP 方案已经移除

这个项目以前有一套本地 socket + HTTP API 的旧方案。

现在当前实现已经不再保留那套代码，真实主流程只看：

- `main.cpp`
- `app_types.h`
- `common.cpp`
- `config_store.cpp`
- `overlay.cpp`
- `ui_launcher.cpp`

理解当前程序运行过程时，不要再按“本地 HTTP 服务”那条旧路线去看。

### 2. README 和 AGENTS 里有些描述已经旧了

仓库说明里还能看到一些“本地服务”“单文件后端”“旧启动方式”的痕迹。

这和现在的真实代码已经不完全一致。

如果说明文档和代码冲突，应该以当前实际参与构建的代码为准。

### 3. `ui_launcher.cpp` 这个名字有点旧

从名字看，好像只是“启动 UI”。

但现在它实际干的事已经很多了：

- 解析命令行
- 初始化 COM
- 创建主窗口
- 初始化 WebView2
- 处理前端桥接消息
- 组织状态 JSON
- 触发退出流程

所以它现在其实已经是“UI 宿主 + 前后端桥接核心模块”。

---

## 你以后顺着代码看，建议按什么顺序

如果你以后想自己对着源码慢慢看，最推荐这个顺序：

1. `src/app_types.h`
   先看程序总状态里到底放了什么。
2. `src/main.cpp`
   看总流程怎么串起来。
3. `src/config_store.cpp`
   看配置从哪来、怎么存。
4. `src/ui_launcher.cpp`
   看前端消息怎么进后端。
5. `src/overlay.cpp`
   看准星怎么真正显示出来。
6. `src/common.cpp`
   最后把零散的小工具补上。

这样看最不容易乱。

因为这相当于：

先看地图，再看总流程，再看细节。

---

## 最后用一句大白话总结

这个项目当前的 C++ 后端运行过程，可以压缩成一句话：

**程序启动后，先把配置和全局状态准备好，再开一个专门负责准星的线程，再开一个装着网页界面的主窗口；用户在页面上的每次操作，都会先改后端内存状态，然后通过 `post_sync -> WM_APP_SYNC -> apply_overlay` 这条链路，把最新状态变成屏幕上的准星显示；退出时再把 UI 和准星线程一起收干净。**

如果你已经能看懂上面这句话，说明你已经抓住这个项目后端的主骨架了。
