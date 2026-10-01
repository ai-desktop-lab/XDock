import QtQuick

Image {
    required property string iconName
    property bool symbolic: false
    property color tint: "white"
    source: "image://system/" + (iconName.startsWith("/") ? "path:" + encodeURIComponent(iconName) : iconName) + (symbolic ? "/" + tint.toString().substring(1) : "")
    sourceSize: Qt.size(64 * Screen.devicePixelRatio, 64 * Screen.devicePixelRatio)
    fillMode: Image.PreserveAspectFit
    smooth: true
}
