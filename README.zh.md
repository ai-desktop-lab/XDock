# XDock

**独立的经典桌面 Dock，与 [XLaunch](https://github.com/ai-workspace-lab/XLaunch) 配合使用。**

[English](README.md) · [设计说明](docs/design-brief.md)

XDock 从用户提供的 2013 DDE 截图中拆分底部栏：负责固定应用、运行状态和窗口切换。XLaunch 负责分类、应用搜索、系统动作与 Agent 指令。两个独立仓库可分别开发、发布与启用。

当前状态：已实现 **C++17 + Qt Quick/QML 原生界面**，Linux 优先，并已补齐 macOS / Windows 应用启动适配边界。支持经典截图外观和系统默认配色、字体及图标。[实现与平台状态](docs/implementation.md) · [视觉核对](design-qa.md)。Linux 系统集成代码已加入，实际桌面环境验证仍待完成。

![原生经典 Dock 渲染](docs/qa/classic-final.png)

**图标规则：两种外观模式均使用系统／应用默认图标。只提取参考图的 Dock 布局与底栏风格，不再使用截图里的 Logo。**

## 编译与运行

依赖 CMake 3.21+、C++17、Qt 6.8+ 的 Quick、QuickControls2、Concurrent、Widgets，以及测试用 QuickTest。菜单和提示使用独立弹窗，不会被细窄的 Dock 窗口裁切。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/xdock --preview
```

Linux 支持 Wayland layer-shell 和 X11/EWMH Dock。Wayland 构建依赖 LayerShellQt，并使用 layer-shell 锚定底部和保留边缘空间；X11 适配面向 ICEWM 等遵循 EWMH 的窗口管理器，通过 `_NET_WM_WINDOW_TYPE_DOCK`、置顶状态和 `_NET_WM_STRUT_PARTIAL` 保留 Dock 区域。Qt 平台主题插件继续提供 GTK 调色板、字体和图标主题兼容，不直接读取 GTK CSS。

macOS 本地预览命令为 `./build/xdock.app/Contents/MacOS/xdock --preview`；正常运行时，Dock 按钮会通过系统 `open -b` 启动已安装的应用。原生窗口管理、托盘服务和系统 Dock 面板替换仍待补齐。

- 右键应用或 `Ctrl+,`：外观设置。
- 方向键与 Enter：选择、打开应用；窄屏自动收纳到“更多应用”。
- `--theme system`：Qt 平台配色、字体与图标；`--theme classic`：截图经典模式。
- `--preview`：仅预览，不启动其他应用，也不保存主题设置。
- `Ctrl+Q`：退出。

可用 `QT_QUICK_BACKEND=software ./build/xdock -platform offscreen --capture classic.png` 保存真实 QML 渲染，使用 `--width 480` 检查窄屏。通过 `cmake --install build --prefix ~/.local` 手动安装，不会替换现有面板。

Dock 固定项由后端模型管理：可在设置中按桌面 ID、macOS Bundle ID 或 Windows 可执行文件添加，右键图标移除，顺序与自定义图标主题名称会持久保存。Linux 使用 `.desktop` 与 `gio` 启动应用，macOS 使用应用 Bundle ID 和系统 `open` 启动应用，Windows 预留常见可执行文件映射。系统托盘服务仍待补齐；X11 窗口管理已接入。

X11 运行应用由真实窗口生成 APP logo，并按应用合并多窗口。右键驻留/移除、拖拽驻留图标排序；XLaunch 右键应用也可驻留。关闭最后一个窗口后，非驻留图标自动消失。默认没有硬编码的 APP 列表。

设置中的“启用 Dock 动画”默认关闭。关闭后会立即停用鱼眼放大、相邻图标位移与 Dock 高度过渡；此选项会在重启后保留。

## macOS 打包

运行 `./packaging/package-macos.sh` 构建 Release、执行测试、打包 Qt 依赖并进行本机 ad-hoc 签名。产物位于 `dist/XDock-0.1.0-macos-arm64/`，包含可独立运行的 `XDock.app` 和 ZIP 包；当前本机版本面向 Apple Silicon、macOS 26+。可将应用复制到 `/Applications/XDock.app` 安装。

打包脚本会校验所有 Mach-O 依赖，避免依赖开发机 Homebrew 路径。此签名用于本机运行，尚未进行 Apple 公证。macOS 应用启动适配已接入；窗口管理、托盘服务等原生能力仍待补齐。

## 琥珀色设计图

![XDock 琥珀色设计图](assets/xdock-amber.png)

使用内置 ImageGen，基于原图和选定的 XLaunch 琥珀色稿制作；这是视觉概念图，尚非可运行 Dock。[生成提示词](assets/prompts.json)。

## 原始参考

![用户提供的 2013 DDE 参考图](assets/dde-2013-reference.png)

视觉沿用原图底部的贴边布局、紧凑彩色图标、轻薄深色透明底栏，与琥珀色桌面搭配。截图中的全屏启动器属于 XLaunch。

XDock 独立运行，不要求 XLaunch 或 Agent 服务；XLaunch 为菜单入口，需要独立安装。目标平台为 macOS 26+、GNOME 50、KDE Plasma 6.6、DDE 7.0、Xfce，均待验证。不依赖 DDE 全家桶。

下一步：在真实 Linux Wayland 合成器验证 layer-shell 与 GTK 平台主题，再补齐 Wayland 运行窗口状态和托盘服务。下方琥珀色图保留为历史概念稿，本次实现以新上传的底栏截图为准。

## X11 应用 Dock

运行任务栏统一使用 APP logo，不再另外排列窗口文字按钮。点击恢复/切换窗口，活动单窗口再次点击最小化；多个窗口合并到一个图标，右键列出窗口并提供驻留、移除、最小化、最大化/还原和关闭。工作区、显示桌面、时钟保留。

XCB 窗口事件以 60 ms 合并更新，15 秒刷新作为恢复机制；默认关闭动画。菜单和显示桌面是会话入口，其他应用来自用户驻留配置和实际窗口。空驻留列表可持久保存。Qt 配置遵循 XDG_CONFIG_HOME，IPC 按显示会话隔离，避免本地与远程桌面串扰。

[设计与交互规范](docs/remote-dock-design.md)。网络、音量与主机状态面板已接入；通用应用托盘协议和 Wayland 窗口枚举单独规划。

## 日期与状态区

右下角两行显示时间和日期，点击打开可切换月份、定位今天的月历。网络、
音量、CPU、内存、磁盘、运行时长与电池收在可折叠面板中；无服务时禁用
相关操作。网络/音量按事件更新，性能仅在面板展开时每 5 秒采样，时钟每
分钟更新。支持窄屏和顶部/底部 Dock。[交互和数据边界](docs/status-calendar.md)。
