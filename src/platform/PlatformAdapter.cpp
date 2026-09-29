#include "PlatformAdapter.h"
#include <QDir>
#include <QMap>
#include <QGuiApplication>
#include <QProcess>
#include <QScreen>
#include <QStandardPaths>
#include <QWindow>
#include <QNativeInterface>
#ifdef XDOCK_X11
#include <xcb/xcb.h>
#include <cstdlib>
#include <cstring>
#endif
#ifdef XDOCK_LAYER_SHELL
#include <LayerShellQt/Shell>
#include <LayerShellQt/Window>
#endif

#ifdef XDOCK_X11
namespace {
xcb_atom_t internAtom(xcb_connection_t *connection, const char *name)
{
    const auto cookie = xcb_intern_atom(connection, 0,
                                        static_cast<uint16_t>(std::strlen(name)), name);
    const auto *reply = xcb_intern_atom_reply(connection, cookie, nullptr);
    if (!reply) return XCB_ATOM_NONE;
    const auto value = reply->atom;
    std::free(const_cast<xcb_intern_atom_reply_t *>(reply));
    return value;
}

void setAtomProperty(xcb_connection_t *connection, xcb_window_t window,
                     xcb_atom_t property, xcb_atom_t value)
{
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, property,
                        XCB_ATOM_ATOM, 32, 1, &value);
}

void setStrut(xcb_connection_t *connection, xcb_window_t window, const QRect &screen,
              bool bottom, uint32_t reserved)
{
    const uint32_t values[] = {
        0, 0, bottom ? 0U : reserved, bottom ? reserved : 0U,
        0, 0, 0, 0,
        static_cast<uint32_t>(qMax(0, screen.left())),
        static_cast<uint32_t>(qMax(0, screen.right())),
        static_cast<uint32_t>(qMax(0, screen.left())),
        static_cast<uint32_t>(qMax(0, screen.right()))
    };
    const auto strut = internAtom(connection, "_NET_WM_STRUT_PARTIAL");
    if (strut != XCB_ATOM_NONE) {
        xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, strut,
                            XCB_ATOM_CARDINAL, 32, 12, values);
    }
}
}
#endif

void PlatformAdapter::initialize(bool layerShell)
{
#ifdef XDOCK_LAYER_SHELL
    if (layerShell) LayerShellQt::Shell::useLayerShell();
#else
    Q_UNUSED(layerShell);
#endif
}

#ifndef Q_OS_MACOS
void PlatformAdapter::configureDockWindow(QWindow *window)
{
#ifdef XDOCK_X11
    if (!window || QGuiApplication::platformName() != "xcb") return;
    auto *x11Application = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    auto *x11Window = window->nativeInterface<QNativeInterface::QX11Window>();
    if (!x11Application || !x11Window) return;
    auto *connection = x11Application->connection();
    const auto windowId = x11Window->handle();
    if (!connection || !windowId) return;

    const auto windowType = internAtom(connection, "_NET_WM_WINDOW_TYPE");
    const auto dockType = internAtom(connection, "_NET_WM_WINDOW_TYPE_DOCK");
    const auto state = internAtom(connection, "_NET_WM_STATE");
    const auto above = internAtom(connection, "_NET_WM_STATE_ABOVE");
    if (windowType != XCB_ATOM_NONE && dockType != XCB_ATOM_NONE)
        setAtomProperty(connection, windowId, windowType, dockType);
    if (state != XCB_ATOM_NONE && above != XCB_ATOM_NONE)
        setAtomProperty(connection, windowId, state, above);

    const QRect screen = window->screen() ? window->screen()->geometry() : QRect();
    const bool bottom = window->y() >= screen.center().y();
    // Keep the reserved desktop area at the visible dock height. The QML
    // hover area expands upward and must not resize maximized applications.
    setStrut(connection, windowId, screen, bottom, 78);
    xcb_flush(connection);
#else
    Q_UNUSED(window);
#endif
}
#endif

QString PlatformAdapter::attach(QWindow *window, int reservedHeight, bool layerShell)
{
    const auto platform = QGuiApplication::platformName();
#ifdef XDOCK_LAYER_SHELL
    if (platform.startsWith("wayland") && layerShell) {
        auto *surface = LayerShellQt::Window::get(window);
        surface->setAnchors(LayerShellQt::Window::AnchorBottom
                            | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight);
        surface->setLayer(LayerShellQt::Window::LayerTop);
        surface->setExclusiveZone(reservedHeight);
        surface->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
        surface->setScope(QStringLiteral("xdock"));
        return QStringLiteral("Wayland layer-shell（需合成器支持）");
    }
#else
    Q_UNUSED(layerShell);
#endif
#ifdef Q_OS_MACOS
    Q_UNUSED(window);
    Q_UNUSED(reservedHeight);
    if (platform == "cocoa")
        return QStringLiteral("macOS：底部贴边窗口适配器");
#endif
#ifdef Q_OS_WIN
    Q_UNUSED(window);
    Q_UNUSED(reservedHeight);
    if (platform == "windows")
        return QStringLiteral("Windows：底部贴边窗口适配器");
#endif
#ifdef XDOCK_X11
    if (platform == "xcb")
        return QStringLiteral("X11 EWMH Dock（ICEWM）");
#endif
    Q_UNUSED(window);
    Q_UNUSED(reservedHeight);
    if (platform.startsWith("wayland"))
        return QStringLiteral("Wayland：未启用 layer-shell，无法作为桌面 Dock 运行");
    return QStringLiteral("%1 预览适配器：尚未实现原生面板集成").arg(platform);
}

QString PlatformAdapter::launch(const QString &application, const QString &launchId)
{
#ifdef Q_OS_LINUX
    // Fixed allowlist; never interpolate desktop Exec fields into a shell.
    const QMap<QString, QStringList> ids = {
        {"launcher", {"xlaunch", "dde-launcher"}},
        {"desktop", {"dde-show-desktop"}},
        {"files", {"dde-file-manager", "org.gnome.Nautilus", "org.kde.dolphin", "thunar"}},
        {"browser", {"firefox", "org.mozilla.firefox", "chromium"}},
        {"music", {"deepin-music", "org.gnome.Music", "rhythmbox"}},
        {"video", {"deepin-movie", "vlc", "org.gnome.Totem"}},
        {"store", {"deepin-app-store", "org.gnome.Software", "org.kde.discover"}},
        {"games", {"deepin-game-center", "steam"}},
        {"screenshot", {"deepin-screen-recorder", "org.kde.spectacle", "org.gnome.Screenshot"}},
        {"terminal", {"deepin-terminal", "org.gnome.Terminal", "org.kde.konsole", "xfce4-terminal"}},
        {"settings", {"dde-control-center", "org.gnome.Settings", "systemsettings"}}
    };
    const QString gio = QStandardPaths::findExecutable("gio");
    if (gio.isEmpty()) return QStringLiteral("缺少 gio，请安装 GLib 工具后重试。");
    const auto candidates = launchId.isEmpty() ? ids.value(application) : QStringList{launchId};
    for (const auto &id : candidates) {
        const auto path = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, id + ".desktop");
        if (path.isEmpty()) continue;
        // Wait for gio's short-lived launcher, not for the launched application.
        QProcess process;
        process.start(gio, {"launch", path});
        if (!process.waitForStarted(1500)) return QStringLiteral("无法启动 gio。");
        if (!process.waitForFinished(3000)) {
            process.kill();
            process.waitForFinished();
            return QStringLiteral("应用启动超时，请从系统启动器重试。");
        }
        if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) return {};
        return QStringLiteral("启动失败：%1").arg(QString::fromUtf8(process.readAllStandardError()).trimmed());
    }
    return QStringLiteral("未找到已安装的对应应用。请先安装应用，再从 Dock 打开。");
#elif defined(Q_OS_MACOS)
    // Use bundle identifiers first so localized macOS app names do not matter.
    const QMap<QString, QStringList> bundles = {
        {"launcher", {"org.aiworkspacelab.xlaunch", "com.apple.launchpad.launcher"}},
        {"desktop", {"com.apple.finder"}},
        {"files", {"com.apple.finder"}},
        {"browser", {"com.apple.Safari", "org.mozilla.firefox", "com.google.Chrome"}},
        {"music", {"com.apple.Music"}},
        {"video", {"com.apple.QuickTimePlayerX", "com.apple.TV"}},
        {"store", {"com.apple.AppStore"}},
        {"games", {"com.apple.Chess", "com.apple.GameCenter"}},
        {"screenshot", {"com.apple.screenshot.launcher", "com.apple.Screenshot"}},
        {"terminal", {"com.apple.Terminal"}},
        {"settings", {"com.apple.systempreferences", "com.apple.SystemPreferences"}}
    };
    const QString open = QStandardPaths::findExecutable("open");
    if (open.isEmpty()) return QStringLiteral("找不到 macOS open 命令。");
    QStringList failures;
    const auto candidates = launchId.isEmpty() ? bundles.value(application) : QStringList{launchId};
    for (const auto &bundle : candidates) {
        QProcess process;
        const bool bundlePath = bundle.endsWith(".app", Qt::CaseInsensitive) && QFileInfo::exists(bundle);
        process.start(open, bundlePath ? QStringList{bundle} : QStringList{"-b", bundle});
        if (!process.waitForStarted(1500))
            return QStringLiteral("无法启动 macOS open 命令。");
        if (!process.waitForFinished(5000)) {
            process.kill();
            process.waitForFinished();
            return QStringLiteral("启动 macOS 应用超时，请稍后重试。");
        }
        if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0)
            return {};
        failures.append(bundle);
    }
    return QStringLiteral("未找到已安装的 macOS 应用（尝试：%1）。")
        .arg(failures.join(QStringLiteral(", ")));
#elif defined(Q_OS_WIN)
    const QMap<QString, QStringList> executables = {
        {"launcher", {"XLaunch.exe", "explorer.exe"}},
        {"desktop", {"explorer.exe"}},
        {"files", {"explorer.exe"}},
        {"browser", {"msedge.exe", "firefox.exe", "chrome.exe"}},
        {"music", {"wmplayer.exe"}},
        {"video", {"wmplayer.exe", "vlc.exe"}},
        {"store", {"explorer.exe"}},
        {"games", {"steam.exe"}},
        {"screenshot", {"SnippingTool.exe"}},
        {"terminal", {"wt.exe", "cmd.exe"}},
        {"settings", {"explorer.exe"}}
    };
    const auto candidates = launchId.isEmpty() ? executables.value(application) : QStringList{launchId};
    for (const auto &executable : candidates) {
        const auto path = QStandardPaths::findExecutable(executable);
        if (path.isEmpty()) continue;
        if (QProcess::startDetached(path, {})) return {};
    }
    return QStringLiteral("未找到已安装的 Windows 应用。");
#else
    Q_UNUSED(application);
    Q_UNUSED(launchId);
    return QStringLiteral("当前平台暂未提供应用启动适配器。");
#endif
}
