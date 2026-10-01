pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import QtQuick.Window

Item {
    id: area
    property var backend: null
    property bool systemTheme: false
    property bool compact: false
    property bool fixedClock: false
    property bool topEdge: false
    signal preferencesRequested()
    signal notice(string message)
    implicitWidth: compact ? 120 : 205
    implicitHeight: 30
    readonly property color foreground: systemTheme ? colors.windowText : "#fff9eb"
    readonly property color secondary: systemTheme ? Qt.rgba(colors.windowText.r, colors.windowText.g, colors.windowText.b, 0.74) : "#dfd0ba"
    readonly property color surface: systemTheme ? colors.window : "#513b27"
    readonly property color hover: systemTheme ? colors.alternateBase : "#604b35"
    readonly property color border: systemTheme ? colors.mid : "#756047"
    readonly property color highlight: systemTheme ? colors.highlight : "#796044"
    readonly property color focusColor: systemTheme ? colors.highlight : "#dfbf8d"
    property int calendarYear: backend ? backend.clock.year : new Date().getFullYear()
    property int calendarMonth: backend ? backend.clock.month : new Date().getMonth() + 1
    property string selectedDate: ""
    property string selectedLabel: ""
    readonly property var monthDays: {
        var today = backend ? backend.clock.isoDate : ""
        return backend ? backend.calendarDays(calendarYear, calendarMonth) : []
    }
    SystemPalette { id: colors }

    function showToday() {
        if (!backend) return
        calendarYear = backend.clock.year
        calendarMonth = backend.clock.month
        selectedDate = backend.clock.isoDate
        selectedLabel = backend.clock.fullDate
        for (var i = 0; i < monthDays.length; ++i) if (monthDays[i].today) calendarGrid.currentIndex = i
    }
    function moveMonth(direction) {
        var date = new Date(calendarYear, calendarMonth - 1 + direction, 1, 12)
        if (date.getFullYear() < 1 || date.getFullYear() > 9999) return
        calendarYear = date.getFullYear()
        calendarMonth = date.getMonth() + 1
        calendarGrid.currentIndex = 0
    }
    function toggleStatus(audioFocus) {
        calendarPopup.close()
        if (statusPopup.visible && !audioFocus) statusPopup.close()
        else {
            statusPopup.audioFocus = audioFocus
            statusPopup.open()
            if (audioFocus) volume.forceActiveFocus()
        }
    }
    component StatusButton: Basic.ToolButton {
        id: control
        property string symbol: ""
        property string iconName: ""
        property string description: ""
        implicitWidth: 28
        implicitHeight: 30
        hoverEnabled: true
        activeFocusOnTab: true
        Accessible.name: description
        ToolTip.visible: hovered && !calendarPopup.visible && !statusPopup.visible
        ToolTip.delay: 600
        ToolTip.text: description
        background: Rectangle { radius: 4; color: control.hovered || control.down ? area.hover : "transparent"; border.color: control.activeFocus ? area.focusColor : "transparent" }
        contentItem: Item {
            Text { anchors.centerIn: parent; text: control.symbol; visible: !control.iconName.length; font.pixelSize: 16; color: control.enabled ? area.foreground : area.secondary }
            SystemIcon { anchors.centerIn: parent; width: 19; height: 19; iconName: control.iconName; visible: control.iconName.length > 0; symbolic: true; tint: control.enabled ? area.foreground : area.secondary }
        }
    }
    component PanelAction: Basic.Button {
        id: action
        implicitHeight: 32
        Accessible.name: text
        contentItem: Text { text: action.text; color: action.enabled ? area.foreground : area.secondary; font.pixelSize: 13; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
        background: Rectangle { radius: 4; color: action.hovered || action.down ? area.highlight : area.hover; border.color: action.activeFocus ? area.focusColor : area.border }
    }
    component Metric: RowLayout {
        id: metric
        property string label: ""
        property string value: ""
        spacing: 12
        Label { Layout.preferredWidth: 66; text: metric.label; color: area.secondary; font.pixelSize: 13 }
        Label { Layout.fillWidth: true; text: metric.value; color: area.foreground; font.pixelSize: 13; wrapMode: Text.Wrap }
    }
    component StatusPopup: Popup {
        popupType: Popup.Window
        width: Math.min(360, Screen.width - 24)
        padding: 16
        palette.window: area.surface
        palette.windowText: area.foreground
        palette.text: area.foreground
        palette.buttonText: area.foreground
        palette.button: area.hover
        palette.highlight: area.highlight
        x: area.width - width
        y: area.topEdge ? area.height + 10 : -height - 10
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { radius: 6; color: area.surface; border.color: area.border }
        enter: Transition {}
        exit: Transition {}
    }

    Row {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 3
        StatusButton {
            objectName: "statusToggle"
            symbol: statusPopup.visible ? "⌄" : "⌃"
            description: statusPopup.visible ? "收起状态面板" : "展开网络与系统状态"
            onClicked: area.toggleStatus(false)
        }
        StatusButton {
            objectName: "networkStatusButton"
            visible: !area.compact
            iconName: area.backend ? area.backend.network.icon : "network-offline"
            description: area.backend ? area.backend.network.name + "\n" + area.backend.network.state : "网络不可用"
            onClicked: area.toggleStatus(false)
        }
        StatusButton {
            objectName: "audioStatusButton"
            visible: !area.compact
            iconName: !area.backend || !area.backend.audio.available || area.backend.audio.muted ? "audio-volume-muted" : "audio-volume-high"
            description: area.backend ? area.backend.audio.text : "音频不可用"
            onClicked: area.toggleStatus(true)
        }
        AbstractButton {
            id: clockButton
            objectName: "clockButton"
            width: area.compact ? 82 : 108
            height: 30
            hoverEnabled: true
            activeFocusOnTab: true
            Accessible.name: "日期与日历：" + (area.backend ? area.backend.clock.fullDate + " " + area.backend.clock.time : "")
            ToolTip.visible: hovered && !calendarPopup.visible && !statusPopup.visible
            ToolTip.delay: 600
            ToolTip.text: area.backend ? area.backend.clock.fullDate + "\n" + area.backend.clock.timezone : ""
            background: Rectangle { radius: 4; color: clockButton.hovered || clockButton.down ? area.hover : "transparent"; border.color: clockButton.activeFocus ? area.focusColor : "transparent" }
            contentItem: Column {
                spacing: 0
                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: area.fixedClock ? "10:38" : area.backend ? area.backend.clock.time : "--:--"; color: area.foreground; font.pixelSize: 13 }
                Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; text: area.fixedClock ? "01-01 周四" : area.backend ? area.backend.clock.date : ""; color: area.secondary; font.pixelSize: 11 }
            }
            onClicked: {
                statusPopup.close()
                if (calendarPopup.visible) calendarPopup.close()
                else { area.backend.refresh(); area.showToday(); calendarPopup.open() }
            }
        }
    }

    StatusPopup {
        id: calendarPopup
        objectName: "calendarPopup"
        height: Math.min(calendarContent.implicitHeight + padding * 2, Screen.height - 100)
        onOpened: calendarGrid.forceActiveFocus()
        contentItem: ScrollView {
            id: calendarScroll
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                id: calendarContent
                width: calendarScroll.availableWidth
                spacing: 10
                Label { Layout.fillWidth: true; text: area.backend ? area.backend.clock.fullDate : ""; color: area.foreground; font.pixelSize: 15; font.bold: true; wrapMode: Text.Wrap }
                Label { Layout.fillWidth: true; text: area.backend ? area.backend.clock.timezone : ""; color: area.secondary; font.pixelSize: 12 }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: area.border }
                RowLayout {
                    Layout.fillWidth: true
                    PanelAction { text: "‹"; implicitWidth: 32; Accessible.name: "上一月"; onClicked: area.moveMonth(-1) }
                    Label { Layout.fillWidth: true; text: area.backend ? area.backend.calendarTitle(area.calendarYear, area.calendarMonth) : ""; horizontalAlignment: Text.AlignHCenter; color: area.foreground; font.pixelSize: 15 }
                    PanelAction { text: "›"; implicitWidth: 32; Accessible.name: "下一月"; onClicked: area.moveMonth(1) }
                }
                Row {
                    Layout.fillWidth: true
                    Repeater {
                        model: area.backend ? area.backend.weekdays : []
                        Label { required property string modelData; width: calendarContent.width / 7; text: modelData; color: area.secondary; horizontalAlignment: Text.AlignHCenter; font.pixelSize: 12 }
                    }
                }
                GridView {
                    id: calendarGrid
                    objectName: "calendarGrid"
                    Layout.fillWidth: true
                    Layout.preferredHeight: cellHeight * 6
                    cellWidth: width / 7
                    cellHeight: 34
                    interactive: false
                    keyNavigationEnabled: false
                    model: area.monthDays
                    delegate: AbstractButton {
                        id: day
                        required property var modelData
                        required property int index
                        width: calendarGrid.cellWidth
                        height: calendarGrid.cellHeight
                        hoverEnabled: true
                        Accessible.name: modelData.label + (modelData.today ? "，今天" : "")
                        contentItem: Text { text: day.modelData.day; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; color: day.modelData.inMonth ? area.foreground : area.secondary; font.pixelSize: 14 }
                        background: Rectangle { anchors.fill: parent; anchors.margins: 2; radius: 4; color: day.modelData.today ? area.highlight : day.hovered ? area.hover : "transparent"; border.color: area.selectedDate === day.modelData.date || calendarGrid.activeFocus && calendarGrid.currentIndex === day.index ? area.focusColor : "transparent" }
                        onClicked: { area.selectedDate = modelData.date; area.selectedLabel = modelData.label; calendarGrid.currentIndex = index; calendarGrid.forceActiveFocus() }
                    }
                    Keys.onPressed: function(event) {
                        var step = event.key === Qt.Key_Left ? -1 : event.key === Qt.Key_Right ? 1 : event.key === Qt.Key_Up ? -7 : event.key === Qt.Key_Down ? 7 : 0
                        if (step) { currentIndex = Math.max(0, Math.min(41, currentIndex + step)); event.accepted = true }
                        else if (event.key === Qt.Key_Return || event.key === Qt.Key_Space) { area.selectedDate = area.monthDays[currentIndex].date; area.selectedLabel = area.monthDays[currentIndex].label; event.accepted = true }
                    }
                }
                Label { Layout.fillWidth: true; text: area.selectedLabel; color: area.secondary; font.pixelSize: 12; wrapMode: Text.Wrap }
                RowLayout {
                    Layout.fillWidth: true
                    PanelAction { text: "今天"; onClicked: area.showToday() }
                    Item { Layout.fillWidth: true }
                    PanelAction { text: "收起"; onClicked: calendarPopup.close() }
                }
            }
        }
    }

    StatusPopup {
        id: statusPopup
        objectName: "systemStatusPopup"
        property bool audioFocus: false
        property bool showInterfaces: false
        height: Math.min(statusContent.implicitHeight + padding * 2, Screen.height - 100)
        onOpened: { if (area.backend) area.backend.panelOpen = true; if (audioFocus) volume.forceActiveFocus() }
        onClosed: if (area.backend) area.backend.panelOpen = false
        contentItem: ScrollView {
            id: statusScroll
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                id: statusContent
                width: statusScroll.availableWidth
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    Label { Layout.fillWidth: true; text: "网络与系统状态"; color: area.foreground; font.bold: true; font.pixelSize: 15 }
                    PanelAction { text: "收起"; onClicked: statusPopup.close() }
                }
                Label { text: "网络"; color: area.secondary; font.pixelSize: 12 }
                Label { Layout.fillWidth: true; text: area.backend ? area.backend.network.name : "不可用"; color: area.foreground; font.pixelSize: 15; elide: Text.ElideRight }
                Label { Layout.fillWidth: true; text: area.backend ? area.backend.network.state : "不可用"; color: area.secondary; font.pixelSize: 12; wrapMode: Text.Wrap }
                Metric { Layout.fillWidth: true; label: "默认路由"; value: area.backend ? area.backend.network.route || "无 IPv4 默认路由" : "不可用" }
                PanelAction {
                    text: (statusPopup.showInterfaces ? "收起网络接口" : "查看网络接口") + "（" + (area.backend ? area.backend.network.interfaces.length : 0) + "）"
                    enabled: !!area.backend && area.backend.network.interfaces.length > 0
                    onClicked: statusPopup.showInterfaces = !statusPopup.showInterfaces
                }
                Repeater {
                    model: statusPopup.showInterfaces && area.backend ? area.backend.network.interfaces : []
                    ColumnLayout {
                        id: link
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 2
                        Label { text: link.modelData.name + (link.modelData.defaultRoute ? " · 默认路由" : ""); color: area.foreground; font.pixelSize: 12 }
                        Label { Layout.fillWidth: true; text: link.modelData.addresses; color: area.secondary; font.pixelSize: 12; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight }
                    }
                }
                PanelAction { text: "网络设置"; enabled: !!area.backend && !!area.backend.tools.network; onClicked: area.backend.openTool("network") }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: area.border }
                RowLayout {
                    Layout.fillWidth: true
                    Label { Layout.fillWidth: true; text: area.backend ? area.backend.audio.text : "音频不可用"; color: area.foreground; font.pixelSize: 13 }
                    PanelAction { text: area.backend && area.backend.audio.muted ? "取消静音" : "静音"; enabled: !!area.backend && area.backend.audio.available; onClicked: area.backend.toggleMute() }
                }
                Basic.Slider {
                    id: volume
                    objectName: "volumeSlider"
                    Layout.fillWidth: true
                    from: 0; to: 100; stepSize: 1
                    enabled: !!area.backend && area.backend.audio.available
                    Accessible.name: "默认输出音量"
                    onMoved: area.backend.setVolume(Math.round(value))
                    background: Rectangle { x: volume.leftPadding; y: volume.topPadding + volume.availableHeight / 2 - height / 2; implicitWidth: 200; implicitHeight: 4; width: volume.availableWidth; height: 4; radius: 2; color: area.border; Rectangle { width: volume.visualPosition * parent.width; height: parent.height; radius: 2; color: area.focusColor } }
                    handle: Rectangle { x: volume.leftPadding + volume.visualPosition * (volume.availableWidth - width); y: volume.topPadding + volume.availableHeight / 2 - height / 2; width: 16; height: 16; radius: 8; color: volume.enabled ? area.foreground : area.secondary; border.color: area.focusColor }
                }
                PanelAction { text: "音频设置"; enabled: !!area.backend && !!area.backend.tools.audio; onClicked: area.backend.openTool("audio") }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: area.border }
                Label { Layout.fillWidth: true; text: "主机运行状态 · " + (area.backend ? area.backend.system.host : ""); color: area.foreground; font.pixelSize: 14; wrapMode: Text.Wrap }
                Metric { Layout.fillWidth: true; label: "CPU"; value: !area.backend || !area.backend.system.available ? "不可用" : area.backend.system.cpuPercent < 0 ? "采样中…" : area.backend.system.cpuPercent + "% · " + area.backend.system.cores + " 核" }
                Metric { Layout.fillWidth: true; label: "内存"; value: area.backend ? area.backend.system.memoryText || "不可用" : "不可用" }
                Metric { Layout.fillWidth: true; label: "负载"; value: area.backend ? area.backend.system.load || "不可用" : "不可用" }
                Metric { Layout.fillWidth: true; label: "磁盘"; value: area.backend ? area.backend.system.diskText || "不可用" : "不可用" }
                Metric { Layout.fillWidth: true; label: "已运行"; value: area.backend ? area.backend.system.uptime || "不可用" : "不可用" }
                Metric { Layout.fillWidth: true; visible: !!area.backend && area.backend.system.batteryPresent; label: "电池"; value: area.backend && area.backend.system.batteryPresent ? area.backend.system.batteryPercent + "% · " + area.backend.system.batteryState : "" }
                Label { Layout.fillWidth: true; text: area.backend ? area.backend.system.os || "此平台暂不提供性能采样" : ""; color: area.secondary; font.pixelSize: 12; wrapMode: Text.Wrap }
                RowLayout {
                    Layout.fillWidth: true
                    PanelAction { Layout.fillWidth: true; text: "系统监视器"; enabled: !!area.backend && !!area.backend.tools.monitor; onClicked: area.backend.openTool("monitor") }
                    PanelAction { Layout.fillWidth: true; text: "Dock 设置"; onClicked: { statusPopup.close(); area.preferencesRequested() } }
                }
            }
        }
    }
    Binding {
        target: volume
        property: "value"
        value: area.backend ? area.backend.audio.volume : 0
        when: !volume.pressed
        restoreMode: Binding.RestoreNone
    }
    Connections {
        target: area.backend
        function onFailure(message) { area.notice(message) }
    }
}
