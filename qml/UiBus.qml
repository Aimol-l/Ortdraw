pragma Singleton
import QtQuick

QtObject {
    property bool snapEnabled: false
    property bool spaceHeld: false
    // 浮层是否打开（打开时画布忽略滚轮缩放）
    property bool overlayOpen: false

    signal contextMenuRequested(real x, real y, string kind, var data)
    signal previewRequested(url src, string uuid)
    signal settingsRequested()
}
