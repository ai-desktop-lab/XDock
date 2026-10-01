import QtQuick
import QtQuick.Controls

ApplicationWindow {
    id: root
    title: dockBackend.theme === "classic" ? "XDock · Classic" : "XDock · System"
    visible: false
    width: previewWidth
    // Keep the original compact dock at rest. The transparent room expands
    // upward only while the fish-eye interaction needs it.
    height: captureMode ? (captureHoverIndex >= 0 ? 132 : 78) : (dock.magnificationActive ? 132 : 78)
    Behavior on height {
        enabled: dockBackend.animationsEnabled && !captureMode
        NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
    }
    minimumWidth: 320
    color: dockBackend.preview ? (dockBackend.theme === "system" ? palette.window : "#d8c49d") : "transparent"
    flags: dockBackend.preview ? Qt.Window : Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    DockBar {
        id: dock
        sessionBackend: sessionActions
        statusBackend: systemStatusBackend
        dockEdge: dockBackend.dockEdge
        anchors.fill: parent
        theme: dockBackend.theme
        animationsEnabled: dockBackend.animationsEnabled
        apps: dockBackend.apps
        fixedClock: captureMode
        interactiveHover: !captureMode
        taskbarMode: !dockBackend.preview && Qt.platform.os === "linux"
        taskWindows: taskbarBackend.windows
        desktopCount: taskbarBackend.desktopCount
        currentDesktop: taskbarBackend.currentDesktop
        onWindowActivated: function(id) { taskbarBackend.activate(id) }
        onWindowMinimized: function(id) { taskbarBackend.minimize(id) }
        onWindowMaximized: function(id) { taskbarBackend.maximize(id) }
        onWindowClosed: function(id) { taskbarBackend.closeWindow(id) }
        onDesktopRequested: function(index) { taskbarBackend.switchDesktop(index) }
        onAppActivated: function(key, name, launchId) {
            if (key === "desktop" && dock.taskbarMode) taskbarBackend.showDesktop()
            else dockBackend.launch(key, name, launchId)
        }
        onRemoveRequested: function(key) { dockBackend.unpinApp(key) }
        onPinRequested: function(key) { dockBackend.pinApp(key) }
        onMoveRequested: function(key, beforeKey) { dockBackend.movePinnedApp(key, beforeKey) }
        onFileDropped: function(path) { dockBackend.pinDesktopFile(path) }
        onPreferencesRequested: { settings.showNormal(); settings.raise(); settings.requestActivate() }
        onStatusRequested: function(name) {
            message.text = name
            notice.show()
        }
    }
    Shortcut { sequence: "Ctrl+,"; onActivated: { settings.showNormal(); settings.raise(); settings.requestActivate() } }
    Shortcut { sequence: "Ctrl+Q"; onActivated: Qt.quit() }
    Connections {
        target: dockBackend
        function onNotice(text) { message.text = text; notice.show() }
    }
    Window {
        id: notice
        title: "XDock"
        width: 380; height: 150
        transientParent: root
        color: palette.window
        SystemPalette { id: palette }
        Label {
            id: message
            anchors.fill: parent; anchors.margins: 24
            wrapMode: Text.Wrap
            color: palette.windowText
            font: Qt.application.font
        }
    }
    Window {
        id: settings
        title: "XDock 设置"
        width: 460; height: 560
        transientParent: root
        color: palette.window
        ScrollView {
         anchors.fill: parent; anchors.margins:24; clip:true
         contentWidth: availableWidth
         Column {
            width: settings.width - 48; spacing:14
            Label { text: "外观"; font.bold: true; font.pixelSize: 18 }
            ComboBox {
                width: parent.width
                model: ["经典 · 2013 DDE", "系统默认 · Qt 平台主题"]
                currentIndex: dockBackend.theme === "system" ? 1 : 0
                onActivated: dockBackend.theme = currentIndex === 1 ? "system" : "classic"
            }
            Label {
                width: parent.width; wrapMode: Text.Wrap
                text: dockBackend.platformStatus
            }
            ComboBox {
                width: parent.width
                model: ["底部常驻（默认）", "顶部常驻"]
                currentIndex: dockBackend.dockEdge === "top" ? 1 : 0
                onActivated: dockBackend.dockEdge = currentIndex === 1 ? "top" : "bottom"
            }
            CheckBox {
                text: "启用 Dock 动画"
                checked: dockBackend.animationsEnabled
                onClicked: dockBackend.animationsEnabled = checked
            }
            Label { text: "驻留应用"; font.bold:true }
            ListView {
                id: pinList
                width:parent.width; height:Math.min(180,count*36); clip:true
                model:dockBackend.pinnedApps
                ScrollBar.vertical:ScrollBar{}
                delegate:Row {
                    required property var modelData
                    required property int index
                    width:pinList.width; height:36; spacing:4
                    Label {width:parent.width-112;height:36;text:modelData.name;verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
                    ToolButton {width:32;height:32;text:"↑";enabled:index>0;Accessible.name:"向左移动";onClicked:dockBackend.movePinnedApp(modelData.key,dockBackend.pinnedApps[index-1].key)}
                    ToolButton {width:32;height:32;text:"↓";enabled:index<pinList.count-1;Accessible.name:"向右移动";onClicked:dockBackend.movePinnedApp(modelData.key,index+2<pinList.count?dockBackend.pinnedApps[index+2].key:"")}
                    ToolButton {width:32;height:32;text:"×";Accessible.name:"移除驻留";onClicked:dockBackend.unpinApp(modelData.key)}
                }
            }
            Label {visible:pinList.count===0;text:"尚未驻留应用，可从菜单或运行图标右键添加。";wrapMode:Text.Wrap;width:parent.width}
            Label { text: "添加固定应用"; font.bold: true }
            TextField {
                id: pinnedName
                width: parent.width
                placeholderText: "显示名称，例如 Firefox"
            }
            TextField {
                id: pinnedLaunchId
                width: parent.width
                placeholderText: "桌面 ID / macOS Bundle ID / Windows 可执行文件"
            }
            TextField {
                id: pinnedIcon
                width: parent.width
                placeholderText: "图标主题名称（可选）"
            }
            Row {
                spacing: 10
                Button {
                    text: "添加并固定"
                    onClicked: {
                        if (dockBackend.addPinnedApp(pinnedName.text, pinnedLaunchId.text, pinnedIcon.text)) {
                            pinnedName.clear()
                            pinnedLaunchId.clear()
                            pinnedIcon.clear()
                        }
                    }
                }
                Button { text: "清空驻留"; onClicked: dockBackend.resetPinnedApps() }
            }
            Label {
                width: parent.width
                wrapMode: Text.Wrap
                opacity: 0.65
                text: "右键应用选择驻留或移除；拖拽驻留图标排序，也可拖入 .desktop 文件。运行中的应用在退出后自动消失。"
            }
            Label { text: "Ctrl+, 设置    ·    Ctrl+Q 退出"; opacity: 0.65 }
            Button { text: "关闭"; onClicked: settings.close() }
         }
        }
    }
}
