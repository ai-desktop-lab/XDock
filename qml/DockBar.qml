import QtQuick
import QtQuick.Controls

Item {
    id: dock
    objectName: "dockBar"
    implicitWidth: 1204
    // Transparent room above the surface keeps enlarged icons from clipping.
    implicitHeight: 132
    property string theme: "classic"
    property bool animationsEnabled: true
    property bool fixedClock: false
    property int focusedIndex: 0
    property int selectedIndex: -1
    property string clockText: fixedClock ? "10:38" : Qt.formatTime(new Date(), "hh:mm")
    property real pointerX: -1
    property bool pointerActive: false
    readonly property bool magnificationActive: animationsEnabled && pointerActive
    readonly property bool systemTheme: theme === "system"
    readonly property real uiScale: Math.min(1, height / 78)
    readonly property real iconSize: 52 * uiScale
    readonly property real slotWidth: 72 * uiScale
    readonly property real trayWidth: 198 * uiScale
    readonly property real magnificationRadius: 170 * uiScale
    readonly property real maxMagnification: 2.05
    property var apps: []
    property bool taskbarMode: false
    property var taskWindows: []
    property int desktopCount: 1
    property int currentDesktop: 0
    signal windowActivated(var id)
    signal windowMinimized(var id)
    signal windowClosed(var id)
    signal windowMaximized(var id)
    signal desktopRequested(int index)
    readonly property int visibleCount: Math.max(0, Math.min(apps.length, taskbarMode ? 4 : apps.length,
        Math.floor((width - trayWidth - 12 * uiScale
                    - (width < apps.length * slotWidth + trayWidth + 12 * uiScale ? 26 * uiScale : 0)) / slotWidth)))
    signal appActivated(string key, string name, string launchId)
    signal removeRequested(int index)
    signal preferencesRequested()
    signal statusRequested(string name)
    property bool keyboardFocus: false

    function iconInfluence(index) {
        if (!magnificationActive || pointerX < 0) return 0
        var center = 14 * uiScale + index * slotWidth + iconSize / 2
        var distance = Math.abs(pointerX - center)
        // A cosine falloff has a zero-slope start and end, avoiding a visible
        // knee as the pointer crosses an icon's influence boundary.
        var normalizedDistance = Math.min(1, distance / magnificationRadius)
        return Math.cos(normalizedDistance * Math.PI / 2)
    }

    function iconMagnification(index) {
        var influence = iconInfluence(index)
        return 1 + (maxMagnification - 1) * influence * influence
    }

    function focalIconIndex() {
        var centerOffset = 14 * uiScale + iconSize / 2
        return Math.max(0, Math.min(visibleCount - 1,
            Math.round((pointerX - centerOffset) / slotWidth)))
    }

    function iconOffset(index) {
        if (!magnificationActive || pointerX < 0 || visibleCount === 0) return 0
        var focalIndex = focalIconIndex()
        if (index === focalIndex) return 0

        // Preserve the normal inter-icon gap. Starting at the focal icon,
        // every enlarged pair contributes half of each extra width to the
        // next centre distance. This makes a smooth, collision-free wave
        // instead of independently sliding icons into one another.
        var start = Math.min(index, focalIndex)
        var end = Math.max(index, focalIndex)
        var displacement = 0
        for (var cursor = start; cursor < end; ++cursor) {
            var leftExpansion = iconSize * (iconMagnification(cursor) - 1)
            var rightExpansion = iconSize * (iconMagnification(cursor + 1) - 1)
            displacement += (leftExpansion + rightExpansion) / 2
        }
        return index < focalIndex ? -displacement : displacement
    }
    Keys.onPressed: function(event) {
        keyboardFocus = true
        if (event.key === Qt.Key_Right) focusedIndex = (focusedIndex + 1) % visibleCount
        else if (event.key === Qt.Key_Left) focusedIndex = (focusedIndex + visibleCount - 1) % visibleCount
        else if (event.key === Qt.Key_Home) focusedIndex = 0
        else if (event.key === Qt.Key_End) focusedIndex = visibleCount - 1
        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) activate(focusedIndex)
        else return
        event.accepted = true
    }

    function activate(index) {
        if (index < 0 || index >= apps.length) return
        selectedIndex = index
        appActivated(apps[index].key, apps[index].name, apps[index].launchId || "")
        pressedTimer.restart()
    }
    SystemPalette { id: systemPalette }
    Timer {
        interval: 30000; running: !dock.fixedClock; repeat: true
        onTriggered: dock.clockText = Qt.formatTime(new Date(), "hh:mm")
    }
    Timer { id: pressedTimer; interval: 800; onTriggered: dock.selectedIndex = -1 }

    Rectangle {
        objectName: "dockSurface"
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 34 * dock.uiScale
        color: dock.systemTheme ? systemPalette.window : "#a63b3931"
        Rectangle {
            anchors.top: parent.top; width: parent.width; height: 1
            color: dock.systemTheme ? systemPalette.mid : "#32eee7cf"
        }
    }

    FocusScope {
        id: applications
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: dock.visibleCount * dock.slotWidth + 12 * dock.uiScale
        height: parent.height
        focus: true
        Repeater {
            model: dock.visibleCount
            delegate: Item {
                id: tile
                required property int index
                readonly property var app: dock.apps[index]
                readonly property real iconMagnification: dock.iconMagnification(index)
                readonly property real baseX: 14 * dock.uiScale + index * dock.slotWidth
                property bool labelArmed: false
                x: baseX + dock.iconOffset(index)
                y: parent.height - 60 * dock.uiScale
                width: dock.iconSize; height: 58 * dock.uiScale
                objectName: "app-" + app.key
                Accessible.role: Accessible.Button
                Accessible.name: app.name
                Accessible.onPressAction: dock.activate(index)
                SystemIcon {
                    iconName: tile.app.icon || "application-x-executable"
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: dock.iconSize * tile.iconMagnification
                    height: width
                    y: parent.height - height
                    opacity: mouse.pressed ? 0.78 : 1
                    Behavior on width { enabled: dock.animationsEnabled; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    Behavior on height { enabled: dock.animationsEnabled; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    Behavior on y { enabled: dock.animationsEnabled; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                }
                Rectangle {
                    anchors.fill: parent; anchors.margins: -3 * dock.uiScale
                    color: "transparent"; radius: 5 * dock.uiScale
                    border.width: dock.activeFocus && dock.focusedIndex === tile.index && dock.keyboardFocus ? 1 : 0
                    border.color: dock.systemTheme ? systemPalette.highlight : "#b7e8f5"
                }
                Rectangle {
                    width: 25 * dock.uiScale; height: 2 * dock.uiScale
                    anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom
                    color: dock.systemTheme ? systemPalette.highlight : "#c2f3fb"
                    visible: dock.selectedIndex === tile.index
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent; hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor
                    onEntered: tile.labelArmed = true
                    onExited: tile.labelArmed = false
                    onClicked: function(event) {
                        dock.focusedIndex = tile.index
                        dock.keyboardFocus = false
                        tile.labelArmed = false
                        if (event.button === Qt.RightButton) appMenu.open()
                        else dock.activate(tile.index)
                    }
                }
                Menu {
                    id: appMenu
                    popupType: Qt.platform.os === "osx" ? Popup.Native : Popup.Window
                    MenuItem {
                        text: "移除固定"
                        visible: !tile.app.transient
                        onTriggered: dock.removeRequested(tile.index)
                    }
                    MenuItem {
                        text: "Dock 设置"
                        onTriggered: dock.preferencesRequested()
                    }
                }
                ToolTip {
                    parent: dock
                    visible: mouse.containsMouse && tile.labelArmed
                    delay: 550
                    text: tile.app.name
                    popupType: Popup.Window
                    x: tile.x + (tile.width - width) / 2
                    y: tile.y - height - 6
                }
                Behavior on x { enabled: dock.animationsEnabled; NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
            }
        }
        // Tracks hover without taking click ownership, so the existing tile
        // MouseAreas keep their activation and context-menu behavior.
        MouseArea {
            id: magnificationTracker
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 78 * dock.uiScale
            z: 20
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            onPositionChanged: function(mouse) {
                if (!mouse) return
                dock.pointerX = mouse.x
                dock.pointerActive = true
            }
            onEntered: function(mouse) {
                if (!mouse) return
                dock.pointerX = mouse.x
                dock.pointerActive = true
            }
            onExited: dock.pointerActive = false
        }
        activeFocusOnTab: true
    }

    ScrollView {
        visible: dock.taskbarMode
        x: applications.width + 8
        width: Math.max(0, tray.x - x - 8)
        height: 36
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        clip: true
        ScrollBar.vertical.policy: ScrollBar.AlwaysOff
        Row {
            spacing: 4
            Repeater {
                model: dock.taskWindows
                Button {
                    id: taskButton
                    required property var modelData
                    width: 140; height: 30
                    text: modelData.title
                    highlighted: modelData.active
                    opacity: modelData.minimized ? 0.65 : 1
                    contentItem: Text {
                        text: taskButton.text
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        color: taskButton.highlighted ? "#ffffff" : "#eee9de"
                    }
                    background: Rectangle {
                        color: taskButton.highlighted ? "#466b85" : "#654943"
                        border.color: taskButton.highlighted ? "#a9d7ed" : "#897568"
                        radius: 3
                    }
                    onClicked: modelData.active ? dock.windowMinimized(modelData.id) : dock.windowActivated(modelData.id)
                    ToolTip.visible: hovered
                    ToolTip.text: modelData.title
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.RightButton
                        onClicked: windowMenu.open()
                    }
                    Menu {
                        id: windowMenu
                        popupType: Popup.Window
                        MenuItem { text: "切换 / 恢复"; onTriggered: dock.windowActivated(taskButton.modelData.id) }
                        MenuItem { text: "最小化"; onTriggered: dock.windowMinimized(taskButton.modelData.id) }
                        MenuItem { text: "最大化 / 还原"; onTriggered: dock.windowMaximized(taskButton.modelData.id) }
                        MenuItem { text: "关闭窗口"; onTriggered: dock.windowClosed(taskButton.modelData.id) }
                    }
                }
            }
        }
    }

    Row {
        id: tray
        anchors.right: parent.right; anchors.rightMargin: 12 * dock.uiScale
        anchors.bottom: parent.bottom; anchors.bottomMargin: 4 * dock.uiScale
        height: 24 * dock.uiScale
        spacing: 7 * dock.uiScale
        ToolButton {
            visible: dock.visibleCount < dock.apps.length
            width: 34 * dock.uiScale; height: parent.height
            text: "更多"
            contentItem: Text {
                text: parent.text
                font.pixelSize: 11 * dock.uiScale
                color: dock.systemTheme ? systemPalette.windowText : "#eee9de"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle { color: "#30ffffff"; radius: 3 }
            Accessible.name: "更多应用"
            onClicked: overflow.open()
            Menu {
                id: overflow
                objectName: "overflowMenu"
                focus: true
                popupType: Qt.platform.os === "osx" ? Popup.Native : Popup.Window
                y: -height
                Repeater {
                    model: Math.max(0, dock.apps.length - dock.visibleCount)
                    MenuItem {
                        required property int index
                        text: dock.apps[dock.visibleCount + index].name
                        onTriggered: dock.activate(dock.visibleCount + index)
                    }
                }
            }
        }
        ToolButton {
            visible: dock.taskbarMode
            text: "桌面 " + (dock.currentDesktop + 1)
            onClicked: desktops.open()
            Menu {
                id: desktops
                popupType: Popup.Window
                y: -height
                Repeater {
                    model: dock.desktopCount
                    MenuItem {
                        required property int index
                        text: "桌面 " + (index + 1)
                        checkable: true
                        checked: index === dock.currentDesktop
                        onTriggered: dock.desktopRequested(index)
                    }
                }
            }
        }
        ToolButton {
            text: "⚙"
            Accessible.name: "Dock 设置"
            onClicked: dock.preferencesRequested()
        }
        AbstractButton {
            width: 46 * dock.uiScale; height: 24 * dock.uiScale
            Accessible.name: "日期与时间"
            contentItem: Text {
                text: dock.clockText
                color: dock.systemTheme ? systemPalette.windowText : "#e4e0dc"
                font.family: Qt.application.font.family
                font.pixelSize: 13 * dock.uiScale
                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                style: dock.systemTheme ? Text.Normal : Text.Raised
                styleColor: "#6b675f"
            }
            onClicked: dock.statusRequested(Qt.formatDate(new Date(), "yyyy年M月d日 dddd"))
        }
    }
}
