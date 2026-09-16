import QtQuick
import QtQuick.Controls
import Theme
import NodeCatalog
import NodeManager

Rectangle {
    id: root

    color: Theme.bgPanel

    property var nodeLayer
    property bool gridMode: false
    property string searchText: ""
    property string activeCat: "all"
    property var collapsedCats: ({})

    signal collapseRequested()

    readonly property var filterCats: {
        var arr = [{ key: "all", name: "全部" }]
        var keys = ["input", "process", "math", "output"]
        for (var i = 0; i < keys.length; i++)
            arr.push({ key: keys[i], name: NodeCatalog.categoryNames[keys[i]] })
        return arr
    }

    readonly property var filteredItems: {
        var q = searchText.toLowerCase()
        return NodeCatalog.items.filter(function(p) {
            var okCat = activeCat === "all" || p.cat === activeCat
            var okQ = !q || p.title.toLowerCase().indexOf(q) >= 0
                   || p.desc.toLowerCase().indexOf(q) >= 0
                   || p.type.toLowerCase().indexOf(q) >= 0
            return okCat && okQ
        })
    }

    readonly property var groupedCats: {
        var order = []
        for (var i = 0; i < filteredItems.length; i++) {
            var c = filteredItems[i].cat
            if (order.indexOf(c) < 0)
                order.push(c)
        }
        return order
    }

    function catItems(cat) {
        return filteredItems.filter(function(p) { return p.cat === cat })
    }

    function toggleCat(cat) {
        var c = {}
        for (var k in collapsedCats)
            c[k] = collapsedCats[k]
        if (c[cat])
            delete c[cat]
        else
            c[cat] = true
        collapsedCats = c
    }

    // Dev helper: create a node instance from the catalog and register it.
    // Returns the created item, or null when the type is not implemented yet.
    function addNodeAt(type, x, y) {
        var url = NodeCatalog.componentUrl(type)
        if (!url) {
            console.warn("节点类型尚未实现，无法添加:", type)
            return null
        }
        if (!nodeLayer) {
            console.warn("NodePalette.nodeLayer 未注入，无法添加节点")
            return null
        }
        var comp = Qt.createComponent(url)
        if (comp.status !== Component.Ready) {
            console.warn("节点组件加载失败:", url, comp.errorString())
            return null
        }
        var props = {}
        if (isFinite(x))
            props.x = x
        if (isFinite(y))
            props.y = y
        var obj = comp.createObject(nodeLayer, props)
        if (!obj) {
            console.warn("节点对象创建失败:", type)
            return null
        }
        if (!NodeManager.createNode(obj)) {
            console.warn("NodeManager.createNode 失败:", type)
            obj.destroy()
            return null
        }
        return obj
    }

    function addNode(type) {
        return addNodeAt(type, 160, 140)
    }

    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.border
    }

    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 42

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 13
            anchors.verticalCenter: parent.verticalCenter
            text: "节点库"
            color: Theme.fgBright
            font.pixelSize: 13
            font.bold: true
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            MiniButton {
                glyph: "☰"
                active: !root.gridMode
                onTriggered: root.gridMode = false
            }
            MiniButton {
                glyph: "▦"
                active: root.gridMode
                onTriggered: root.gridMode = true
            }
            MiniButton {
                glyph: "‹"
                onTriggered: root.collapseRequested()
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.borderSoft
        }
    }

    TextField {
        id: search
        anchors.top: header.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.right: parent.right
        anchors.rightMargin: 12
        height: 32
        placeholderText: "搜索节点…"
        placeholderTextColor: Theme.fgDim
        color: Theme.fg
        font.pixelSize: 12
        leftPadding: 28
        rightPadding: 26
        selectByMouse: true
        onTextChanged: root.searchText = text

        background: Rectangle {
            color: Theme.bg
            radius: 8
            border.width: 1
            border.color: search.activeFocus ? Theme.blue : Theme.border
        }
    }

    Text {
        anchors.left: search.left
        anchors.leftMargin: 10
        anchors.verticalCenter: search.verticalCenter
        text: "⌕"
        color: Theme.fgDim
        font.pixelSize: 14
    }

    Rectangle {
        id: clearButton
        visible: search.text.length > 0
        width: 20
        height: 20
        radius: 5
        anchors.right: search.right
        anchors.rightMargin: 6
        anchors.verticalCenter: search.verticalCenter
        color: clearArea.containsMouse ? Theme.bgHover : "transparent"

        Text {
            anchors.centerIn: parent
            text: "×"
            color: clearArea.containsMouse ? Theme.fgBright : Theme.fgDim
            font.pixelSize: 15
        }

        MouseArea {
            id: clearArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: search.text = ""
        }
    }

    Flow {
        id: filters
        anchors.top: search.bottom
        anchors.topMargin: 9
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 6

        Repeater {
            model: root.filterCats

            delegate: Rectangle {
                id: chip
                required property var modelData
                readonly property bool active: root.activeCat === modelData.key
                width: chipText.implicitWidth + 20
                height: 24
                radius: 12
                color: active ? Qt.rgba(Theme.blue.r, Theme.blue.g, Theme.blue.b, 0.14) : Theme.bg
                border.width: 1
                border.color: active ? Qt.rgba(Theme.blue.r, Theme.blue.g, Theme.blue.b, 0.36) : Theme.border

                Text {
                    id: chipText
                    anchors.centerIn: parent
                    text: chip.modelData.name
                    color: chip.active ? Theme.blue : Theme.fgDim
                    font.pixelSize: 11
                    font.bold: chip.active
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.activeCat = chip.modelData.key
                }
            }
        }
    }

    Flickable {
        id: paletteView
        anchors.top: filters.bottom
        anchors.topMargin: 6
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        contentWidth: width
        contentHeight: contentColumn.height + 20
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentColumn
            width: paletteView.width
            height: implicitHeight

            Repeater {
                model: root.groupedCats

                delegate: Column {
                    id: groupCol
                    required property string modelData
                    width: contentColumn.width
                    height: implicitHeight
                    readonly property bool collapsed: root.collapsedCats[modelData] === true
                    readonly property int count: root.catItems(modelData).length

                    Item {
                        width: groupCol.width
                        height: 30

                        Row {
                            anchors.left: parent.left
                            anchors.leftMargin: 6
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 6

                            Canvas {
                                id: caret
                                width: 9
                                height: 9
                                anchors.verticalCenter: parent.verticalCenter
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    ctx.fillStyle = Theme.fgDim
                                    ctx.beginPath()
                                    if (groupCol.collapsed) {
                                        ctx.moveTo(1, 0)
                                        ctx.lineTo(6, 4.5)
                                        ctx.lineTo(1, 9)
                                    } else {
                                        ctx.moveTo(0, 1)
                                        ctx.lineTo(9, 1)
                                        ctx.lineTo(4.5, 6)
                                    }
                                    ctx.closePath()
                                    ctx.fill()
                                }

                                Connections {
                                    target: groupCol
                                    function onCollapsedChanged() { caret.requestPaint() }
                                }
                                Connections {
                                    target: Theme
                                    function onChanged() { caret.requestPaint() }
                                }
                                Component.onCompleted: requestPaint()
                            }

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: NodeCatalog.categoryNames[groupCol.modelData] || groupCol.modelData
                                color: Theme.fgDim
                                font.pixelSize: 10
                                font.bold: true
                                font.letterSpacing: 0.9
                            }

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: countText.implicitWidth + 12
                                height: 16
                                radius: 5
                                color: Theme.bg
                                border.width: 1
                                border.color: Theme.borderSoft

                                Text {
                                    id: countText
                                    anchors.centerIn: parent
                                    text: groupCol.count
                                    color: Theme.fgDim
                                    font.pixelSize: 10
                                    font.bold: true
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.toggleCat(groupCol.modelData)
                        }
                    }

                    Grid {
                        id: body
                        width: groupCol.width
                        columns: root.gridMode ? 2 : 1
                        spacing: root.gridMode ? 8 : 2
                        visible: !groupCol.collapsed
                        height: childrenRect.height

                        Repeater {
                            model: root.catItems(groupCol.modelData)

                            delegate: PaletteItem {
                                required property var modelData
                                entry: modelData
                                gridMode: root.gridMode
                                width: root.gridMode ? (body.width - body.spacing) / 2 : body.width
                                onAddRequested: (type) => root.addNode(type)
                            }
                        }
                    }
                }
            }
        }
    }

    Text {
        visible: root.filteredItems.length === 0
        anchors.top: filters.bottom
        anchors.topMargin: 40
        anchors.horizontalCenter: parent.horizontalCenter
        text: "没有匹配的节点"
        color: Theme.fgDim
        font.pixelSize: 12
    }

    component MiniButton: Rectangle {
        id: mb

        property string glyph
        property bool active: false

        signal triggered()

        width: 22
        height: 22
        radius: 6
        color: active ? Qt.rgba(Theme.blue.r, Theme.blue.g, Theme.blue.b, 0.12)
                      : (mbArea.containsMouse ? Theme.bgHover : "transparent")

        Text {
            anchors.centerIn: parent
            text: mb.glyph
            color: mb.active ? Theme.blue : Theme.fgDim
            font.pixelSize: 13
        }

        MouseArea {
            id: mbArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: mb.triggered()
        }
    }
}
