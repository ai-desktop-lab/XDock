# Native implementation · 2026-09-05

## Scope and visual truth

The latest user-supplied strip, `assets/reference/dock-strip.png` (1204 × 78), supersedes the amber concept mockup for this implementation. The user's confirmed stack is **C++17 + Qt Quick/QML**, with Linux first and future macOS/Windows adapters. This is a native application, not a website or screenshot overlay.

Classic geometry at 1×: a 78 px transparent window, a 34 px bottom surface, 52 × 52 px icons starting at (14, 18), and a 72 px application pitch (20 px between 52 px slots, tightened from 80 px at the user’s request). Eleven separate interactive application items precede the right-aligned system area. Below the available width, additional entries move into a native menu rather than overlapping the system controls.

The latest user clarification is authoritative: extract the Dock style, but use system-default application logos. Both appearance modes now resolve installed application icons / the host icon theme, with standard platform icons as fallback. The reference PNG is not bundled into the executable and no screenshot icon, reflection or logo is used at runtime. Application icons keep their original proportions and colors; tray symbols use the current foreground tint. QML still owns all layout and interaction.

The wallpaper and the clipped launcher labels above the reference's dock belong to the desktop, not XDock. Production mode has a transparent upper region. Preview mode uses a neutral amber backdrop for inspecting alpha edges; it does not recreate or install a wallpaper or launcher. A pale underline is transient activation feedback, not an asserted running-window state.

## Modules

- `qml/DockBar.qml`: independent visual component; semantic activation signals, keyboard control, overflow, accessible application names.
- `src/SystemIcons.h`: host application / theme / standard platform icon provider.
- `src/DockBackend.*`: persisted appearance, edge placement and mutable pinned-app model; preview never launches applications or saves preferences.
- `src/platform/PlatformAdapter.*`: Linux `.desktop` discovery and `gio launch`, Wayland layer-shell, X11/EWMH Dock properties, and macOS Bundle ID launching. Fixed candidate IDs, no shell evaluation of `Exec` values. The Cocoa window adapter keeps the Dock visible across app activation, Spaces and fullscreen transitions.
- `src/main.cpp`: screen geometry, startup flags and native render capture.

## Theme contract

`classic` preserves the reference surface color and geometry. `system` additionally follows `SystemPalette` and the Qt application font. **Both modes use system icons**, independently of the surface theme. Linux reads the installed desktop entry's `Icon` field, then the icon theme; macOS reads native application bundle icons. Missing icons fall back to Qt platform standard icons, never the reference screenshot. Tray symbols are tinted for readable foreground contrast. The interface remains Qt Quick; Qt Widgets supplies native standard-icon services only.

GTK integration is through the Qt platform theme installed by the distribution (for example its GTK platform-theme package or qt6ct). GTK CSS is not loaded into QML, and the application does not force an environment variable or alter the desktop's theme. On macOS, Qt's native platform palette/font/control style is used. Windows is reserved by the adapter boundary; a native Windows dock adapter is not implemented.

## Platform status

| Environment | Implemented | Validation in this task |
| --- | --- | --- |
| Native Qt UI | Classic/system, click, hover, keyboard, overflow, settings, capture | Built with Qt 6.11.2 on macOS; Qt Quick tests and actual native UI interaction checked |
| Linux application launching | Desktop-entry candidates with GLib gio | Source implemented; Linux runtime verification pending |
| Linux Wayland | Required LayerShellQt, bottom anchors and exclusive zone | Source implemented; compatible compositor validation pending |
| Linux X11 / ICEWM | EWMH Dock type, above state and partial strut reservation | Source implemented; ICEWM runtime validation pending |
| macOS / Windows | Portable UI, application launch adapters and platform status boundary | macOS launches installed apps by Bundle ID via `open`; the Dock uses a native status-level window that remains visible when another app activates; Windows resolves common executables |

Layer-shell integration follows the [KDE LayerShellQt API](https://github.com/KDE/layer-shell-qt). XDock requires a compositor that supports this protocol; it is not a universal GNOME Wayland panel implementation.

Running-window enumeration/switching, StatusNotifierItem hosting, system volume/input/user controls, pin reordering, autohide, live GTK theme testing and mixed-DPI multi-monitor validation remain outside this visual implementation. Pinned app addition/removal and persistence are implemented. Tray clicks explicitly report that the service is not connected.

## Verification

`ctest --test-dir build --output-on-failure` exercises real pointer and keyboard activation, narrow layout, invalid index handling, and theme switching. `--capture` renders the real Qt Quick surface at a fixed clock state and exits with an error if saving fails. See `../design-qa.md` for comparison evidence.

The Fedora CI workflow compiles Linux-specific branches and captures the native surface on future pushes/PRs. It has been added but has not been run remotely in this task.
