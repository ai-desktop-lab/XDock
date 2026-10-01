#pragma once
#include <QQuickImageProvider>
#include <QApplication>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QIcon>
#include <QPainter>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QStyle>
#include <QUrl>
#include <QMap>
#include <memory>
#ifdef XDOCK_X11
#include <xcb/xcb.h>
#include <cstdlib>
#include <cstring>
#endif

// Appearance and application branding are independent: both dock themes use
// host icons, and the reference screenshot is never an icon fallback.
class SystemIcons final : public QQuickImageProvider {
public:
    SystemIcons() : QQuickImageProvider(QQuickImageProvider::Image) {}
    ~SystemIcons() override {
#ifdef XDOCK_X11
        if (m_connection) xcb_disconnect(m_connection);
#endif
    }
    QImage requestImage(const QString &id, QSize *size, const QSize &requested) override {
        const auto parts = id.split('/');
        const QString name = parts.value(0);
        QIcon icon;
#ifdef XDOCK_X11
        if (name.startsWith("window:")) {
            if (!m_connection) m_connection = xcb_connect(nullptr, nullptr);
            if (!xcb_connection_has_error(m_connection)) {
                if (!m_iconAtom) {
                    auto *reply = xcb_intern_atom_reply(m_connection, xcb_intern_atom(m_connection, 0, 12, "_NET_WM_ICON"), nullptr);
                    if (reply) { m_iconAtom = reply->atom; std::free(reply); }
                }
                auto *reply = xcb_get_property_reply(m_connection, xcb_get_property(m_connection, 0, name.mid(7).toUInt(), m_iconAtom, XCB_ATOM_CARDINAL, 0, 262144), nullptr);
                if (reply && reply->format == 32) {
                    auto *pixels = static_cast<const quint32 *>(xcb_get_property_value(reply));
                    int length = xcb_get_property_value_length(reply) / 4;
                    int best = -1, bestSize = 100000;
                    for (int offset = 0; offset + 2 < length;) {
                        quint32 w = pixels[offset], h = pixels[offset+1];
                        quint64 count = quint64(w) * h;
                        if (!w || !h || count > quint64(length-offset-2)) break;
                        int score = qAbs(int(w)-64) + (w<32 ? 1000 : 0);
                        if (w <= 256 && h <= 256 && score < bestSize) { best = offset; bestSize = score; }
                        offset += int(count) + 2;
                    }
                    if (best >= 0) {
                        QImage image(reinterpret_cast<const uchar *>(pixels+best+2), pixels[best], pixels[best+1], QImage::Format_ARGB32);
                        icon = QIcon(QPixmap::fromImage(image.copy()));
                    }
                }
                std::free(reply);
            }
        }
#endif
        if (name.startsWith("path:")) {
            const auto path = QUrl::fromPercentEncoding(name.mid(5).toUtf8());
            if (path.endsWith(".desktop")) {
                QSettings desktop(path, QSettings::IniFormat);
                auto name = desktop.value("Desktop Entry/Icon").toString();
                icon = QFileInfo(name).isAbsolute() ? QIcon(name) : QIcon::fromTheme(name);
            } else if (QFileInfo::exists(path)) icon = QIcon(path);
            if (icon.isNull() && QFileInfo::exists(path) && qobject_cast<QApplication *>(qApp))
                icon = QFileIconProvider().icon(QFileInfo(path));
        }
#ifdef Q_OS_MACOS
        const QMap<QString, QString> bundles = {
            {"view-app-grid", "/System/Applications/Launchpad.app"},
            {"user-desktop", "/System/Library/CoreServices/Finder.app"},
            {"system-file-manager", "/System/Library/CoreServices/Finder.app"},
            {"firefox", "/Applications/Safari.app"},
            {"multimedia-audio-player", "/System/Applications/Music.app"},
            {"multimedia-video-player", "/System/Applications/QuickTime Player.app"},
            {"system-software-install", "/System/Applications/App Store.app"},
            {"applications-games", "/System/Applications/Games.app"},
            {"applets-screenshooter", "/System/Applications/Utilities/Screenshot.app"},
            {"utilities-terminal", "/System/Applications/Utilities/Terminal.app"},
            {"preferences-system", "/System/Applications/System Settings.app"}
        };
        const auto path = bundles.value(name);
        if (icon.isNull() && parts.size() == 1 && !path.isEmpty() && QFileInfo::exists(path) && qobject_cast<QApplication *>(qApp))
            icon = QFileIconProvider().icon(QFileInfo(path));
#endif
#ifdef Q_OS_LINUX
        const QMap<QString, QStringList> desktopIds = {
            {"view-app-grid", {"xlaunch", "dde-launcher"}},
            {"system-file-manager", {"dde-file-manager", "org.gnome.Nautilus", "org.kde.dolphin", "thunar"}},
            {"firefox", {"firefox", "org.mozilla.firefox", "chromium"}},
            {"multimedia-audio-player", {"deepin-music", "org.gnome.Music", "rhythmbox"}},
            {"multimedia-video-player", {"deepin-movie", "vlc", "org.gnome.Totem"}},
            {"system-software-install", {"deepin-app-store", "org.gnome.Software", "org.kde.discover"}},
            {"applications-games", {"deepin-game-center", "steam"}},
            {"applets-screenshooter", {"deepin-screen-recorder", "org.kde.spectacle", "org.gnome.Screenshot"}},
            {"utilities-terminal", {"deepin-terminal", "org.gnome.Terminal", "org.kde.konsole", "xfce4-terminal"}},
            {"preferences-system", {"dde-control-center", "org.gnome.Settings", "systemsettings"}}
        };
        for (const auto &desktopId : (parts.size() == 1 ? desktopIds.value(name) : QStringList{})) {
            const auto path = QStandardPaths::locate(QStandardPaths::ApplicationsLocation, desktopId + ".desktop");
            if (path.isEmpty()) continue;
            QSettings desktop(path, QSettings::IniFormat);
            const auto iconName = desktop.value("Desktop Entry/Icon").toString();
            icon = QFileInfo(iconName).isAbsolute() ? QIcon(iconName) : QIcon::fromTheme(iconName);
            break;
        }
#endif
        if (icon.isNull()) icon = QIcon::fromTheme(name);
        if (icon.isNull()) {
            const QMap<QString, QStyle::StandardPixmap> defaults = {
                {"view-app-grid", QStyle::SP_ComputerIcon},
                {"user-desktop", QStyle::SP_DesktopIcon},
                {"system-file-manager", QStyle::SP_DirHomeIcon},
                {"firefox", QStyle::SP_DriveNetIcon},
                {"multimedia-audio-player", QStyle::SP_MediaVolume},
                {"multimedia-video-player", QStyle::SP_MediaPlay},
                {"system-software-install", QStyle::SP_DialogSaveButton},
                {"preferences-system", QStyle::SP_FileDialogDetailedView},
                {"audio-volume-high", QStyle::SP_MediaVolume},
                {"audio-volume-muted", QStyle::SP_MediaVolumeMuted},
                {"network-wired", QStyle::SP_DriveNetIcon},
                {"network-wireless", QStyle::SP_DriveNetIcon},
                {"network-offline", QStyle::SP_DriveNetIcon},
                {"dialog-information", QStyle::SP_MessageBoxInformation},
                {"input-keyboard", QStyle::SP_ComputerIcon},
                {"avatar-default", QStyle::SP_DirHomeIcon}
            };
            // QuickTest uses QGuiApplication; its fallback style is test-local.
            std::unique_ptr<QStyle> fallback;
            QStyle *style = nullptr;
            if (qobject_cast<QApplication *>(qApp)) style = QApplication::style();
            else { fallback.reset(QStyleFactory::create("Fusion")); style = fallback.get(); }
            if (style) icon = style->standardIcon(defaults.value(name, QStyle::SP_FileIcon));
        }
        const QSize dimensions = requested.isValid() ? requested : QSize(52, 52);
        QImage result(dimensions, QImage::Format_ARGB32_Premultiplied);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        icon.paint(&painter, result.rect(), Qt::AlignCenter);
        if (parts.size() > 1) {
            painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
            painter.fillRect(result.rect(), QColor("#" + parts[1]));
        }
        if (size) *size = dimensions;
        return result;
    }
private:
#ifdef XDOCK_X11
    xcb_connection_t *m_connection = nullptr;
    xcb_atom_t m_iconAtom = 0;
#endif
};
