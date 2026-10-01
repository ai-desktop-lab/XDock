#include "SessionActions.h"
#include "DockBackend.h"
#include "Taskbar.h"
#include "SystemIcons.h"
#include "platform/PlatformAdapter.h"
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>
#include <QImage>
#include <QDebug>
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>
#include <memory>

int main(int argc, char *argv[]) {
    bool requestedLayerShell = false;
    bool requestedPreview = false;
    for (int i = 1; i < argc; ++i) {
        const QByteArray argument(argv[i]);
        requestedLayerShell |= argument == "--layer-shell";
        requestedPreview |= argument == "--preview" || argument == "--capture" || argument.startsWith("--capture=");
    }
#ifdef Q_OS_LINUX
    requestedLayerShell = !requestedPreview;
#endif
    // Lock before constructing QApplication: duplicate launches need no GUI or portal connection.
    std::unique_ptr<QLockFile> instanceLock;
    if (!requestedPreview) {
        QString session = qEnvironmentVariable("DISPLAY", qEnvironmentVariable("WAYLAND_DISPLAY", "default"));
        if (!qEnvironmentVariable("DISPLAY").isEmpty()) session = session.section('.', 0, 0);
        session.replace(QRegularExpression("[^a-zA-Z0-9_-]"), "_");
        QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if (runtime.isEmpty()) runtime = QDir::tempPath() + "/xdock-" + QString::number(qHash(QDir::homePath()));
        QDir().mkpath(runtime);
        instanceLock = std::make_unique<QLockFile>(runtime + "/xdock-" + session + ".lock");
        instanceLock->setStaleLockTime(0);
        if (!instanceLock->tryLock(0)) return 0;
    }
    // Select the Wayland shell integration before Qt creates its platform client.
    PlatformAdapter::initialize(requestedLayerShell && !requestedPreview);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName("ai-workspace-lab");
    app.setApplicationName("XDock");
    app.setApplicationVersion("0.1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("Linux-first classic Qt Quick dock");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"preview", "Open a standalone visual preview; never launch applications."});
    parser.addOption({"capture", "Save a deterministic 1204x78 native render and exit.", "file"});
    parser.addOption({"hover-index", "With --capture, render a deterministic fish-eye hover state (0-10).", "index"});
    parser.addOption({"width", "Preview width in logical pixels (minimum 320).", "pixels", "1204"});
    parser.addOption({"theme", "Appearance: classic or system.", "theme"});
    parser.process(app);
    bool ok = false;
    const int requestedWidth = parser.value("width").toInt(&ok);
    if (!ok || requestedWidth < 320 || requestedWidth > 16384) {
        qCritical() << "--width must be between 320 and 16384";
        return 2;
    }
    const QString theme = parser.value("theme");
    if (!theme.isEmpty() && theme != "classic" && theme != "system") {
        qCritical() << "--theme must be classic or system";
        return 2;
    }
    bool hoverIndexOk = true;
    const int hoverIndex = parser.isSet("hover-index")
        ? parser.value("hover-index").toInt(&hoverIndexOk) : -1;
    if (!hoverIndexOk || hoverIndex < -1 || hoverIndex > 10 ||
        (hoverIndex >= 0 && !parser.isSet("capture"))) {
        qCritical() << "--hover-index requires --capture and must be between 0 and 10";
        return 2;
    }
    const bool preview = parser.isSet("preview") || parser.isSet("capture");
#ifdef Q_OS_LINUX
    const auto platformName = QGuiApplication::platformName();
    if (!preview && platformName != "xcb" && !platformName.startsWith("wayland")) {
        qCritical() << "XDock requires an X11 (xcb) or Wayland session on Linux.";
        return 3;
    }
#endif
    const bool layerShell = !preview && QGuiApplication::platformName().startsWith("wayland");
    Taskbar taskbar;
    DockBackend backend(preview);
    if (!preview && !backend.ownsEndpoint()) return 0;
    if (!preview && QGuiApplication::platformName() == "xcb") {
        QObject::connect(&taskbar, &Taskbar::changed, &backend, [&] { backend.updateWindows(taskbar.windows()); });
        backend.updateWindows(taskbar.windows());
    }
    if (!theme.isEmpty()) backend.setTheme(theme);
    SessionActions sessionActions(preview);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("sessionActions", &sessionActions);
    engine.addImageProvider("system", new SystemIcons);
    engine.rootContext()->setContextProperty("dockBackend", &backend);
    engine.rootContext()->setContextProperty("taskbarBackend", &taskbar);
    engine.rootContext()->setContextProperty("previewWidth", requestedWidth);
    engine.rootContext()->setContextProperty("captureMode", parser.isSet("capture"));
    engine.load(QUrl("qrc:/qml/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window) return 1;
    if (hoverIndex >= 0) {
        auto *dock = window->findChild<QObject *>(QStringLiteral("dockBar"));
        if (!dock) return 1;
        const qreal scale = qMin<qreal>(1, window->height() / 78.0);
        dock->setProperty("pointerX", (14 + hoverIndex * 72 + 26) * scale);
        dock->setProperty("pointerActive", true);
    }
    if (!preview) {
        const auto position = [window, &backend, layerShell] {
            const QRect screen = window->screen()->geometry();
            window->setWidth(screen.width());
            const int y = backend.dockEdge() == "top"
                ? screen.top()
                : screen.bottom() - window->height() + 1;
            window->setPosition(screen.x(), y);
            // The expanded hover area is transparent; reserve only the
            // visible dock surface's original footprint on desktop panels.
            backend.setPlatformStatus(PlatformAdapter::attach(window, 78, layerShell));
            if (window->isVisible()) PlatformAdapter::configureDockWindow(window);
        };
        QObject::connect(&backend, &DockBackend::dockEdgeChanged, window, position);
        QObject::connect(window, &QWindow::heightChanged, window, [position](int) { position(); });
        position();
        QObject::connect(window, &QWindow::screenChanged, &app, [position](QScreen *) { position(); });
        QObject::connect(window->screen(), &QScreen::geometryChanged, &app, [position](const QRect &) { position(); });
    } else {
        backend.setPlatformStatus("独立视觉预览 · Linux 优先");
    }
    if (!preview) PlatformAdapter::configureDockWindow(window);
    window->show();
    if (!preview) PlatformAdapter::configureDockWindow(window);
    if (parser.isSet("capture")) {
        QTimer::singleShot(600, &app, [&] {
            const QImage capture = window->grabWindow().scaled(window->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            if (capture.isNull() || !capture.save(parser.value("capture"))) {
                qCritical() << "Failed to capture Qt Quick window";
                app.exit(1);
            } else app.quit();
        });
    }
    return app.exec();
}
