# XDock

**A compact classic desktop dock, designed alongside [XLaunch](https://github.com/ai-workspace-lab/XLaunch).**

[中文](README.zh.md) · [Design brief](docs/design-brief.md)

XDock is the independently developed dock component inspired by the bottom bar of the user-supplied 2013 DDE Classic desktop reference. It owns pinned applications, running indicators and window switching. XLaunch owns categories, application search, system actions and Agent commands.

Status: native **C++17 + Qt Quick/QML** implementation. Linux first; macOS and Windows application-launch adapters are included, while native window management remains platform-specific work. Classic screenshot styling and a system palette/font/icon mode are available. Linux desktop integration is implemented but still needs live Linux validation. [Implementation and platform status](docs/implementation.md) · [Visual QA](design-qa.md).

![Native classic Dock render](docs/qa/classic-final.png)

**Icon policy:** both themes use system/application icons. The reference supplies Dock geometry and surface styling only; no screenshot logos are bundled.

## Build and run

Requires CMake 3.21+, C++17, and Qt 6.8+ (Quick, Quick Controls 2, Concurrent, Widgets; QuickTest for tests). Separate popup windows keep menus and tooltips outside the thin dock surface.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/xdock --preview
```

On Linux, XDock supports both Wayland and X11. When LayerShellQt is available,
the Wayland layer-shell backend anchors the Dock and reserves its screen edge;
on ICEWM/X11, the XCB backend publishes EWMH dock type, above state and partial
struts. A Qt platform-theme plugin supplies GTK palette, font and icon-theme
compatibility; XDock does not parse GTK CSS.

For headless VPS and containers, run the X11 backend inside Xvfb. It uses CPU
rendering and does not require a GPU or compositor; X11VNC/noVNC can export the
virtual display separately.

On macOS, the preview executable is `./build/xdock.app/Contents/MacOS/xdock --preview`; normal mode launches installed apps through bundle identifiers and `/usr/bin/open`. Native window switching and Dock replacement integration remain separate adapter work.

Add a pinned application in settings with a desktop ID, macOS Bundle ID or Windows executable; right-click a Dock icon to remove it. Pinned entries and custom icon-theme names persist across restarts. Left/right arrows and Enter activate an application. `Ctrl+Q` quits. Preview mode never starts other apps. Use `--theme system` for platform palette/fonts/icons, or `--theme classic` for the screenshot appearance.

Capture: `QT_QUICK_BACKEND=software ./build/xdock -platform offscreen --capture classic.png`. Add `--width 480` to inspect overflow. Installation is opt-in via `cmake --install build --prefix ~/.local`; it does not replace the desktop panel.

## Amber design concept

![XDock amber concept](assets/xdock-amber.png)

Built-in ImageGen concept, grounded in the reference below and the selected XLaunch amber design. This is a visual mockup, not a running dock. [Generation prompts](assets/prompts.json).

## Visual reference

![User-supplied 2013 DDE reference](assets/dde-2013-reference.png)

Use the bottom strip as the reference: edge-aligned placement, compact colorful icons and a restrained dark translucent surface over warm amber wallpaper. The full-screen launcher in this image belongs to the companion XLaunch product.

## Independent operation

- XDock must work without XLaunch or an Agent runtime.
- XLaunch may appear as an optional pinned entry.
- Shared app identity and activation contracts are planned; no hard runtime dependency.
- Target macOS 26+, GNOME 50, KDE Plasma 6.6, DDE 7.0 and Xfce through separate adapters. These are design targets, not verified support.
- No DDE suite dependency. Desktop coexistence and window-control permissions require platform validation.

Next: validate the Wayland layer-shell and GTK platform-theme integration on real Linux compositors, then implement running-window state and system tray services. The historical concept below remains a reference; the latest supplied strip is the visual source of truth for the implemented classic theme.
