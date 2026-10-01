# 日期、日历和可折叠状态区

## 默认交互

保持现有贴边 Dock 与琥珀色弹窗。右下角保留用户/工作区、状态展开箭头、
网络与音量图标，以及两行时间/日期。点击时钟打开月历：按本机 locale 的
星期起始日排列 42 个日期，支持上一月、下一月、今天定位、方向键选择，
今天和选中日期分别突出。跨月日期仍可选择，Esc/点击外部/收起关闭。
时区读取运行主机设置，不写死亚洲时区；悬停时钟显示完整日期与时区。

展开箭头或网络图标打开一个可滚动面板，音量图标打开面板并聚焦滑块。
日历与状态面板互斥。窄屏隐藏常驻网络/音量和工作区文字，保留展开入口、
用户与日期，避免挤掉左侧应用菜单。弹窗分别从底部向上、顶部向下展开。
状态数据是运行 XDock 的主机数据，远程会话看到的是远端主机信息。

## 数据与能力

| 项目 | 来源与边界 |
| --- | --- |
| 时间/日期 | QDateTime、QLocale、系统时区，每分钟边界更新，跨午夜更新 |
| 网络 | NetworkManager D-Bus 属性与服务事件；内核接口/路由是降级来源 |
| 默认路由 | Linux IPv4 路由的最低 metric；接口地址在二级折叠区列出，包括隧道接口 |
| 互联网状态 | 仅在 NetworkManager 配置了检查且有结果时报告；接口 UP 不代表互联网可用 |
| 音量 | 当前用户默认输出，wpctl 优先、pactl 降级；显式 PULSE_SERVER 会话使用 pactl；静音/滑块是明确用户动作 |
| CPU/内存/负载/运行时长 | Linux /proc；CPU 使用计数差值，初次显示“采样中” |
| 磁盘 | 用户主目录所在文件系统的可用空间，不扫描文件 |
| 电池 | Linux /sys/class/power_supply；不存在时隐藏 |
| 设置工具 | 查找已安装的网络编辑器、pavucontrol、系统监视器；缺失时禁用 |

容器里的 /proc、路由和电池可见范围由宿主机/命名空间决定，目前不是
cgroup 限额监视器。非 Linux 暂不提供主机性能与电池数据，日期和日历仍可用。
通用 StatusNotifierItem/XEmbed 应用托盘协议另行规划，此面板不冒充其实现。

## 性能与执行约束

- 没有秒级常驻时钟、模糊、阴影动画或性能曲线。
- 网络订阅 NetworkManager PropertiesChanged、服务重启与 netlink 路由/
  地址事件，250ms 合并。只读取已有连通性结果，不自行探测外部网址。
- 音量用一个 pactl subscribe 事件流，150ms 合并；没有事件流时仅在展开
  面板期间更新。服务不可用时显示不可用，打开面板可重新连接。
- 主机性能启动读取一次，展开时每 5 秒采样；关闭后停止。磁盘/系统读取
  在 QtConcurrent 工作线程进行，GUI 线程不等待外部命令。
- 所有音频命令异步、3 秒超时，滑块更新 180ms 合并；用户音量设置限制
  0–100%，不使用 shell 拼接或 sudo，不更改网络连接。
- 截图模式关闭真实指针悬停并固定高度，避免进入/退出悬停造成尺寸重绘循环。
- 预览模式不读主机性能、网络或音量，也不启动工具/修改音量。
- Linux 的 Qt DBus 为组件已有依赖。Debian 包明确声明使用的 QML runtime
  模块，音频与设置工具仅为建议依赖。

## 接口依据

[NetworkManager](https://networkmanager.dev/docs/api/latest/gdbus-org.freedesktop.NetworkManager.html)
与 [ActiveConnection](https://networkmanager.dev/docs/api/latest/gdbus-org.freedesktop.NetworkManager.Connection.Active.html)。
音频参数按目标主机 wpctl / pactl 自带帮助核对。

## 交付范围

编译与运行界面检查不等于完整网络/音频故障注入验收。构建产物部署前保留
旧二进制备份；本轮不重启显示管理器、退出用户会话、切换网络或修改系统时间。
仓库测试按现有 GitHub CI 执行，不新增测试套件。
