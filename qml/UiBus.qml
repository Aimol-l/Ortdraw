pragma Singleton
import QtQuick

QtObject {
    property bool snapEnabled: false
    property bool spaceHeld: false

    signal contextMenuRequested(real x, real y, string kind, var data)
}
