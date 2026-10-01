# 2026-10-01 Home-Ubuntu 部署记录

目标：`shenlan@10.79.0.7`，源代码 `/home/shenlan/workspaces/XDock` 与 `/home/shenlan/workspaces/XLaunch`。

## 已执行

- 两个 Qt 项目在远端编译成功；git diff --check、ICEWM 启动脚本语法检查通过。
- 独立 Xvfb `:98` + ICEWM（关闭原生任务栏）：菜单 520×560，Dock 1280×78，菜单底边与 Dock 顶边重合。
- 两个临时 Qt 客户端窗口使用同一 WM_CLASS：Dock 合并为一个 APP 图标，右键列出两个窗口。
- `.desktop` 驻留 IPC 返回 ok，配置写入 Qt QSettings；拖拽两个驻留图标前后排序，配置顺序随操作更新。
- 驻留图标在窗口关闭后保留，运行短线消失；窗口列表不显示 XDock/XLaunch。
- 重复启动 XDock 返回 0，没有第二个 Dock；XLaunch toggle 使用已有实例。
- F11 进入 1280×800 全屏，Esc 返回 520×560 左下角菜单。
- 私有会话内 3 秒空闲采样：XDock、XLaunch 均 0.0% CPU（按进程 jiffies 计算）；这是短时空闲样本，不是 RDP 吞吐或持续操作基准。

## 当前远程会话

已原子替换 `~/.local/bin/xdock`、`~/.local/bin/xlaunch`，只重启 DISPLAY `:10.0` 中这两个进程。KWin、Plasma、浏览器及本地显示会话保留。远程配置继续使用 `~/.config/ai-desktop-remote`，Qt Quick 软件渲染、Dock 动画关闭。

实际会话菜单：520×560 +0+210；Dock：1352×78 +0+770；最大化 Chrome：1352×770，未覆盖 Dock。两端日志无 QML 错误。KDE 启动脚本中 XLaunch 改为 `--hidden`，由 Dock 按需打开。

原始旧程序备份：`~/.local/state/task-ai-desktop/backup-polish-20261001-145108/`。后续配置界面完善前的程序备份：`~/.local/state/task-ai-desktop/backup-polish-20261001-145408/`。配置界面已支持滚动查看驻留项、移动和移除，避免窄屏溢出后无法管理。部署截图：`~/.local/state/task-ai-desktop/deployed-menu.png`。

SHA256：

- xdock: `1f73192076d71c3c62475b9bbfcd4f38c392ab8c62c72cb857c917577a88e0ea`
- xlaunch: `df632a7501aed48712b1979fc010cb443d78fe05eda2a4e736dc3d0fe2eb4554`

## 范围

覆盖 X11 的 IceWM 和当前 KWin 远程会话。Wayland 任务枚举、完整系统托盘、网络与音量服务仍待后续实现。私有 D-Bus 会话出现主机 inotify 配额/portal 注册警告；当前远程会话两端日志为空。本次没有修改系统配额，也没有构建 ISO 或发布新的 Debian 包。
