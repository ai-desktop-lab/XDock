import QtQuick
import QtQuick.Controls

Item {
    id: dock
    property var root: dock
    property var sessionBackend: null
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
    readonly property real trayWidth: (198 + (sessionBackend ? 40 : 0)) * uiScale
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
    readonly property int visibleCount: Math.max(0, Math.min(apps.length, apps.length,
        Math.floor((width - trayWidth - 12 * uiScale
                    - (width < apps.length * slotWidth + trayWidth + 12 * uiScale ? 26 * uiScale : 0)) / slotWidth)))
    signal appActivated(string key, string name, string launchId)
    signal removeRequested(string key)
    signal pinRequested(string key)
    signal moveRequested(string key, string beforeKey)
    signal fileDropped(url path)
    property string draggingKey: ""
    property real dragStartX: 0
    property int dragTargetIndex: -1
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
        if (visibleCount === 0) return
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
        var app = apps[index]
        var windows = app.windows || []
        if (windows.length === 1) {
            if (windows[0].active) windowMinimized(windows[0].id)
            else windowActivated(windows[0].id)
        } else if (windows.length > 1) {
            // Cycle a group instead of launching another process.
            var next = 0
            for (var i = 0; i < windows.length; ++i) if (windows[i].active) next = (i + 1) % windows.length
            windowActivated(windows[next].id)
        } else appActivated(app.key, app.name, app.launchId || "")
        pressedTimer.restart()
    }
    SystemPalette { id: systemPalette }
    component DockMenuItem: MenuItem {
        id: action
        height: 32
        implicitWidth: 300
        contentItem: Text { text:action.text; font:action.font; color:!action.enabled ? "#b4a28d" : dock.systemTheme ? action.highlighted ? systemPalette.highlightedText : systemPalette.text : "#fff9eb"; elide:Text.ElideRight; verticalAlignment:Text.AlignVCenter; leftPadding:action.checkable?22:0 }
        background: Rectangle { radius:3; color: action.highlighted ? dock.systemTheme ? systemPalette.highlight : "#796044" : "transparent" }
    }
    component DockMenu: Menu {
        width: 300
        palette.window: dock.systemTheme ? systemPalette.window : "#513b27"
        palette.text: dock.systemTheme ? systemPalette.text : "#fff9eb"
        palette.buttonText: dock.systemTheme ? systemPalette.buttonText : "#fff9eb"
        palette.highlight: dock.systemTheme ? systemPalette.highlight : "#796044"
        palette.highlightedText: "#fff9eb"
        background: Rectangle { implicitWidth:300; implicitHeight:40; color: dock.systemTheme ? systemPalette.window : "#513b27"; border.color: dock.systemTheme ? systemPalette.mid : "#756047"; radius:4 }
    }
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
                readonly property var app: dock.apps[index] || {}
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
                    opacity: dock.draggingKey === tile.app.key ? 0.5 : mouse.pressed ? 0.78 : 1
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
                    color: tile.app.active ? "#c2f3fb" : "#c7b28b"
                    visible: (tile.app.windows || []).length > 0 || dock.selectedIndex === tile.index
                }
                MouseArea {
                    id: mouse
                    anchors.fill: parent; hoverEnabled: true
                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                    cursorShape: Qt.PointingHandCursor
                    onEntered: tile.labelArmed = true
                    onExited: tile.labelArmed = false
                    property bool moved: false
                    onPressed: function(event) { dock.dragStartX = mapToItem(dock, event.x, event.y).x; moved = false }
                    onPositionChanged: function(event) {
                        if (pressed && (pressedButtons & Qt.LeftButton) && tile.app.pinned) {
                            if (Math.abs(mapToItem(dock, event.x, event.y).x - dock.dragStartX) > 10) { moved = true; dock.draggingKey = tile.app.key; dock.dragTargetIndex = Math.max(2, Math.min(dock.apps.length, Math.floor((mapToItem(dock,event.x,event.y).x - 14*dock.uiScale)/dock.slotWidth) + (mapToItem(dock,event.x,event.y).x > dock.dragStartX ? 1 : 0))) }
                        }
                    }
                    onReleased: function(event) {
                        if (moved) {
                            var target = dock.dragTargetIndex
                            dock.moveRequested(tile.app.key, target >= dock.visibleCount || !dock.apps[target] || !dock.apps[target].pinned ? "" : dock.apps[target].key)
                        }
                        dock.draggingKey = ""
                    }
                    onCanceled: dock.draggingKey = ""
                    onClicked: function(event) {
                        if (moved) return
                        dock.focusedIndex = tile.index
                        dock.keyboardFocus = false
                        tile.labelArmed = false
                        if (event.button === Qt.RightButton) appMenu.open()
                        else dock.activate(tile.index)
                    }
                }
                DockMenu {
                    id: appMenu
                    popupType: Qt.platform.os === "osx" ? Popup.Native : Popup.Window
                    DockMenuItem {
                        text: tile.app.pinned ? "从 Dock 移除驻留" : "在 Dock 中驻留"
                        visible: !tile.app.utility
                        height: tile.app.utility ? 0 : 32
                        enabled: !!tile.app.launchId
                        onTriggered: tile.app.pinned ? dock.removeRequested(tile.app.key) : dock.pinRequested(tile.app.key)
                    }
                    DockMenuItem {
                        text: "打开新窗口"
                        visible: !tile.app.utility && !!tile.app.launchId
                        height: !tile.app.utility && !!tile.app.launchId ? 32 : 0
                        onTriggered: dock.appActivated(tile.app.key, tile.app.name, tile.app.launchId)
                    }
                    MenuSeparator { visible: (tile.app.windows || []).length > 0; height:(tile.app.windows || []).length > 0 ? 8 : 0 }
                    Repeater {
                        model: tile.app.windows || []
                        DockMenuItem {
                            required property var modelData
                            text: (modelData.active ? "● " : "") + modelData.title
                            onTriggered: dock.windowActivated(modelData.id)
                        }
                    }
                    DockMenuItem { text: "最小化窗口"; visible: (tile.app.windows || []).length > 0; height:(tile.app.windows || []).length > 0 ? 32 : 0; onTriggered: { for (var w of tile.app.windows) dock.windowMinimized(w.id) } }
                    DockMenuItem { text: "最大化 / 还原"; visible: (tile.app.windows || []).length === 1; height:(tile.app.windows || []).length === 1 ? 32 : 0; onTriggered: dock.windowMaximized(tile.app.windows[0].id) }
                    DockMenuItem { text: "关闭窗口"; visible: (tile.app.windows || []).length > 0; height:(tile.app.windows || []).length > 0 ? 32 : 0; onTriggered: { for (var w of tile.app.windows) dock.windowClosed(w.id) } }
                    DockMenuItem {
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

    DropArea {
        anchors.fill: parent
        onEntered: function(drag) { drag.accepted = drag.hasUrls }
        onDropped: function(drop) { for (var path of drop.urls) dock.fileDropped(path) }
    }
    Rectangle {
        visible: dock.draggingKey.length > 0
        x: 10*dock.uiScale + dock.dragTargetIndex*dock.slotWidth; anchors.bottom: parent.bottom
        width:2; height:60*dock.uiScale; color: "#c7b28b"
    }

    Row {
        id: tray
        anchors.right: parent.right; anchors.rightMargin: 12 * dock.uiScale
        anchors.bottom: parent.bottom; anchors.bottomMargin: 4 * dock.uiScale
        height: 24 * dock.uiScale
        spacing: 7 * dock.uiScale
        ToolButton {
            palette.buttonText: dock.systemTheme ? systemPalette.buttonText : "#eee9de"
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
            DockMenu {
                id: overflow
                objectName: "overflowMenu"
                focus: true
                popupType: Qt.platform.os === "osx" ? Popup.Native : Popup.Window
                y: -height
                Repeater {
                    model: Math.max(0, dock.apps.length - dock.visibleCount)
                    DockMenuItem {
                        required property int index
                        text: dock.apps[dock.visibleCount + index].name
                        onTriggered: dock.activate(dock.visibleCount + index)
                    }
                }
            }
        }
        ToolButton {
            objectName:"userMenuButton"
            visible:!!dock.sessionBackend
            width:30*dock.uiScale;height:parent.height
            Accessible.name:"用户与会话："+(sessionBackend?sessionBackend.userName:"")
            ToolTip.visible:hovered;ToolTip.text:sessionBackend?sessionBackend.displayName:"";ToolTip.delay:600
            contentItem:Rectangle {
                radius:height/2;color:dock.systemTheme?systemPalette.mid:"#796044"
                Image {anchors.fill:parent;source:sessionBackend?sessionBackend.avatar:"";visible:source.toString().length>0;fillMode:Image.PreserveAspectCrop;sourceSize:Qt.size(32,32)}
                Text {anchors.centerIn:parent;text:sessionBackend?sessionBackend.userName.slice(0,1).toUpperCase():"";visible:!sessionBackend||sessionBackend.avatar.toString().length===0;color:dock.systemTheme?systemPalette.windowText:"#fff9eb";font.pixelSize:13}
            }
            onClicked:userMenu.open()
            DockMenu {
                id:userMenu;objectName:"userSessionMenu";popupType:Popup.Window;y:-height
                onAboutToShow:if(sessionBackend)sessionBackend.refresh()
                DockMenuItem {text:sessionBackend?sessionBackend.displayName+" · 用户信息":"";onTriggered:profileDialog.open()}
                MenuSeparator {}
                Repeater {
                    model:sessionBackend?sessionBackend.actions:[]
                    DockMenuItem {
                        required property var modelData
                        text:modelData.text;enabled:modelData.enabled
                        ToolTip.visible:hovered&&!enabled;ToolTip.text:modelData.reason;ToolTip.delay:450
                        onTriggered:sessionBackend.request(modelData.key)
                    }
                }
            }
        }
        ToolButton {
            palette.buttonText: dock.systemTheme ? systemPalette.buttonText : "#eee9de"
            visible: dock.taskbarMode
            text: "桌面 " + (dock.currentDesktop + 1)
            contentItem: Text {text:parent.text;color:dock.systemTheme?systemPalette.buttonText:"#eee9de";font.pixelSize:13;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
            onClicked: desktops.open()
            DockMenu {
                id: desktops
                popupType: Popup.Window
                y: -height
                Repeater {
                    model: dock.desktopCount
                    DockMenuItem {
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
            palette.buttonText: dock.systemTheme ? systemPalette.buttonText : "#eee9de"
            text: "⚙"
            contentItem: Text {text:parent.text;color:dock.systemTheme?systemPalette.buttonText:"#eee9de";font.pixelSize:14;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
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

    Dialog {
        id: sessionDialog
        objectName: "sessionConfirmation"
        property string actionKey: ""
        property string detail: ""
        width: Math.min(420, root.width - 24)
        modal: true; popupType: Popup.Window
        x:(root.width-width)/2;y:-height-24
        title: "会话操作"
        standardButtons: Dialog.Ok | Dialog.Cancel
        palette.window: dock.systemTheme?systemPalette.window:"#513b27"
        palette.text: dock.systemTheme?systemPalette.windowText:"#fff9eb"
        palette.windowText: dock.systemTheme?systemPalette.windowText:"#fff9eb"
        palette.buttonText: dock.systemTheme?systemPalette.windowText:"#fff9eb"
        palette.button: dock.systemTheme?systemPalette.button:"#604b35"
        palette.highlight: dock.systemTheme?systemPalette.highlight:"#796044"
        background: Rectangle {color:dock.systemTheme?systemPalette.window:"#513b27";border.color:dock.systemTheme?systemPalette.mid:"#756047";radius:6}
        contentItem: Label {text:sessionDialog.detail;color:dock.systemTheme?systemPalette.windowText:"#fff9eb";wrapMode:Text.Wrap;font.pixelSize:14;lineHeight:1.3}
        onOpened: {standardButton(Dialog.Ok).text=actionKey==="poweroff"?"关闭主机":actionKey==="reboot"?"重启主机":actionKey==="login"?"退出并重新登录":"注销";standardButton(Dialog.Cancel).text="取消";standardButton(Dialog.Cancel).forceActiveFocus()}
        onAccepted: sessionBackend.confirm(actionKey)
        onClosed: sessionBackend.cancel()
    }
    Dialog {
        id: profileDialog
        objectName: "userInformation"
        width:Math.min(420,root.width-24);modal:true;popupType:Popup.Window
        x:(root.width-width)/2;y:-height-24
        title:"当前用户";standardButtons:Dialog.Close
        palette.window:dock.systemTheme?systemPalette.window:"#513b27";palette.text:dock.systemTheme?systemPalette.windowText:"#fff9eb";palette.windowText:dock.systemTheme?systemPalette.windowText:"#fff9eb";palette.buttonText:dock.systemTheme?systemPalette.windowText:"#fff9eb";palette.button:dock.systemTheme?systemPalette.button:"#604b35"
        background:Rectangle {color:dock.systemTheme?systemPalette.window:"#513b27";border.color:dock.systemTheme?systemPalette.mid:"#756047";radius:6}
        contentItem:Label {text:sessionBackend?sessionBackend.displayName+"\n账号："+sessionBackend.userName+"\n"+sessionBackend.sessionLabel:"";color:dock.systemTheme?systemPalette.windowText:"#fff9eb";wrapMode:Text.Wrap;font.pixelSize:14;lineHeight:1.4}
        onOpened:standardButton(Dialog.Close).text="关闭"
    }
    Connections {
        target:sessionBackend
        function onConfirmationRequested(key,title,detail){sessionDialog.actionKey=key;sessionDialog.title=title;sessionDialog.detail=detail;sessionDialog.open()}
        function onFailure(text){dock.statusRequested(text)}

    }
}
