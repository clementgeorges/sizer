import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1100
    height: 740
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: qsTr("Sizer")
    color: theme.background

    // Light themes get light controls; the palette comes from the Omarchy theme.
    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.accent: theme.accent
    Material.background: theme.background
    Material.foreground: theme.foreground
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    property bool quitConfirmed: false
    property string savedFolder: ""
    property int savedCount: 0

    // The width typed into the field, or 0 while it isn't a number.
    readonly property int typedWidth: {
        const value = parseInt(widthField.text)
        return isNaN(value) ? 0 : value
    }

    function number(value) {
        translator.language
        return value.toLocaleString(Qt.locale(), 'f', 0)
    }

    function applyWidth() {
        if (win.typedWidth > 0)
            backend.setWidth(win.typedWidth)
        grid.forceActiveFocus()
    }

    function quit() {
        if (backend.dirty && !win.quitConfirmed)
            quitDialog.open()
        else
            Qt.quit()
    }

    onClosing: (close) => {
        if (backend.dirty && !win.quitConfirmed) {
            close.accepted = false
            quitDialog.open()
        }
    }

    // Keeps the field showing the width of what the next change applies to.
    function syncWidth() {
        if (!widthField.activeFocus)
            widthField.text = backend.commonWidth > 0 ? String(backend.commonWidth) : ""
    }
    Connections {
        target: backend
        function onChanged() { win.syncWidth() }
        function onSaved(folder, count) {
            win.savedFolder = folder
            win.savedCount = count
            savedToast.show()
        }
    }

    Shortcut { sequences: ["?", "Shift+?"]; context: Qt.ApplicationShortcut; onActivated: win.helpVisible = !win.helpVisible }
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; onActivated: win.quit() }
    Shortcut { sequences: ["A", StandardKey.Open]; context: Qt.ApplicationShortcut; onActivated: backend.addImages() }
    Shortcut { sequences: ["Space", StandardKey.Save]; context: Qt.ApplicationShortcut; enabled: !backend.busy; onActivated: backend.save() }
    Shortcut { sequences: [StandardKey.SelectAll]; context: Qt.ApplicationShortcut; onActivated: backend.selectAll() }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; onActivated: backend.undo() }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; context: Qt.ApplicationShortcut; onActivated: backend.redo() }
    Shortcut { sequences: ["Delete", "Backspace"]; context: Qt.ApplicationShortcut; onActivated: backend.removeSelected() }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; onActivated: backend.resetSize() }
    Shortcut {
        sequence: "W"
        context: Qt.ApplicationShortcut
        onActivated: { widthField.forceActiveFocus(); widthField.selectAll() }
    }
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (win.helpVisible)
                win.helpVisible = false
            else if (widthField.activeFocus) {
                grid.forceActiveFocus()
                win.syncWidth()
            } else
                backend.selectNone()
        }
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // Top bar: add images and see what's in the batch.
            RowLayout {
                Layout.fillWidth: true
                Layout.margins: 12
                spacing: 12

                AccentButton {
                    text: qsTr("Add Images")
                    filled: backend.count === 0
                    enabled: !backend.busy
                    onClicked: backend.addImages()
                }
                Label {
                    color: theme.muted
                    text: backend.count === 0 ? ""
                          : backend.selectedCount > 0
                            ? qsTr("%n images", "", backend.count) + "  ·  " + qsTr("%n selected", "", backend.selectedCount)
                            : qsTr("%n images", "", backend.count)
                }
                Item { Layout.fillWidth: true }
                Button {
                    flat: true
                    text: qsTr("Select All")
                    visible: backend.count > 0
                    onClicked: backend.selectAll()
                }
                Button {
                    flat: true
                    text: qsTr("Remove")
                    visible: backend.selectedCount > 0
                    enabled: !backend.busy
                    onClicked: backend.removeSelected()
                }
                Label {
                    color: theme.muted
                    text: qsTr("? for shortcuts")
                }
            }

            // Problems are said in the window, not swallowed.
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                visible: backend.error !== ""
                implicitHeight: errorRow.implicitHeight + 16
                radius: Math.min(4, theme.radius)
                color: Qt.alpha(theme.error, 0.15)
                border.color: theme.error

                RowLayout {
                    id: errorRow
                    anchors.fill: parent
                    anchors.margins: 8
                    Label {
                        Layout.fillWidth: true
                        text: backend.error
                        wrapMode: Text.Wrap
                        color: theme.foreground
                    }
                    Button {
                        flat: true
                        text: qsTr("Dismiss")
                        onClicked: backend.dismissError()
                    }
                }
            }

            // The batch.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                GridView {
                    id: grid
                    anchors.fill: parent
                    anchors.margins: 6
                    clip: true
                    focus: true
                    model: backend.images
                    boundsBehavior: Flickable.StopAtBounds
                    currentIndex: -1
                    // Fill the width with whole tiles about 220 wide.
                    cellWidth: width / Math.max(1, Math.floor(width / 220))
                    cellHeight: 250
                    ScrollBar.vertical: ScrollBar {}

                    delegate: PhotoTile {
                        width: grid.cellWidth
                        height: grid.cellHeight
                        current: GridView.isCurrentItem && grid.activeFocus
                        onClicked: (row, modifiers) => {
                            grid.currentIndex = row
                            grid.forceActiveFocus()
                            backend.select(row, (modifiers & Qt.ControlModifier) !== 0, (modifiers & Qt.ShiftModifier) !== 0)
                        }
                    }

                    // Clicking between tiles clears the selection.
                    TapHandler {
                        onTapped: (point) => {
                            if (grid.indexAt(point.position.x + grid.contentX, point.position.y + grid.contentY) < 0) {
                                backend.selectNone()
                                grid.forceActiveFocus()
                            }
                        }
                    }

                    Keys.onPressed: (event) => {
                        const before = grid.currentIndex
                        if (before < 0 && backend.count > 0 &&
                            [Qt.Key_Left, Qt.Key_Right, Qt.Key_Up, Qt.Key_Down].includes(event.key)) {
                            grid.currentIndex = 0
                        } else if (event.key === Qt.Key_Left) grid.moveCurrentIndexLeft()
                        else if (event.key === Qt.Key_Right) grid.moveCurrentIndexRight()
                        else if (event.key === Qt.Key_Up) grid.moveCurrentIndexUp()
                        else if (event.key === Qt.Key_Down) grid.moveCurrentIndexDown()
                        else if (event.key === Qt.Key_Home) grid.currentIndex = 0
                        else if (event.key === Qt.Key_End) grid.currentIndex = backend.count - 1
                        else return
                        event.accepted = true
                        backend.select(grid.currentIndex, false, (event.modifiers & Qt.ShiftModifier) !== 0)
                    }
                }

                // Nothing yet: say how to start.
                ColumnLayout {
                    anchors.centerIn: parent
                    visible: backend.count === 0
                    spacing: 14

                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Drop images here")
                        font.pixelSize: 26
                        color: theme.foreground
                    }
                    Label {
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("or press A to choose them")
                        color: theme.muted
                    }
                }

                DropArea {
                    id: drop
                    anchors.fill: parent
                    keys: ["text/uri-list"]
                    onDropped: (event) => {
                        backend.addPaths(event.urls.map(url => url.toString()))
                        event.acceptProposedAction()
                    }
                }
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 12
                    visible: drop.containsDrag
                    color: Qt.alpha(theme.accent, 0.08)
                    border.color: theme.accent
                    border.width: 2
                    radius: theme.radius
                }
            }

            // The new size, and saving.
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: bottomRow.implicitHeight + 24
                color: theme.surface

                RowLayout {
                    id: bottomRow
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    spacing: 12
                    enabled: backend.count > 0 && !backend.busy

                    TextField {
                        id: widthField
                        Layout.preferredWidth: 130
                        placeholderText: qsTr("Width (px)")
                        inputMethodHints: Qt.ImhDigitsOnly
                        validator: IntValidator { bottom: 1; top: 30000 }
                        horizontalAlignment: Text.AlignHCenter
                        onAccepted: win.applyWidth()
                        onActiveFocusChanged: if (!activeFocus) win.syncWidth()
                    }
                    Label {
                        color: theme.muted
                        text: {
                            // heightFor() depends on what's selected and how many images there are.
                            backend.selectedCount; backend.count
                            if (win.typedWidth <= 0)
                                return qsTr("Height follows the width")
                            const height = backend.heightFor(win.typedWidth)
                            // Every image keeps its own shape, so a mixed batch has no single height.
                            return height > 0 ? qsTr("× %1 px").arg(win.number(height))
                                              : qsTr("Height follows each image's shape")
                        }
                    }

                    Repeater {
                        model: [640, 1024, 1280, 1920]
                        delegate: Button {
                            required property int modelData
                            flat: true
                            text: win.number(modelData)
                            onClicked: { widthField.text = String(modelData); win.applyWidth() }
                        }
                    }

                    Button {
                        text: backend.selectedCount > 0 ? qsTr("Apply to %n Selected", "", backend.selectedCount)
                                                        : qsTr("Apply to All")
                        enabled: win.typedWidth > 0
                        onClicked: win.applyWidth()
                    }
                    Button {
                        flat: true
                        text: qsTr("Original Size")
                        onClicked: backend.resetSize()
                    }

                    Item { Layout.fillWidth: true }

                    ProgressBar {
                        Layout.preferredWidth: 160
                        visible: backend.busy
                        value: backend.progress
                    }
                    AccentButton {
                        text: backend.busy ? qsTr("Saving…") : qsTr("Save to Folder…")
                        onClicked: backend.save()
                    }
                }
            }
        }

        // After a save: where it went, and a way to look.
        Rectangle {
            id: savedToast
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 90
            width: Math.min(parent.width - 32, toastRow.implicitWidth + 32)
            height: toastRow.implicitHeight + 20
            radius: theme.radius
            color: theme.surface
            border.color: theme.accent
            opacity: 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: 180 } }

            function show() { opacity = 1; hideTimer.restart() }
            Timer { id: hideTimer; interval: 7000; onTriggered: savedToast.opacity = 0 }

            RowLayout {
                id: toastRow
                anchors.centerIn: parent
                width: Math.min(implicitWidth, savedToast.width - 32)
                spacing: 12
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    color: theme.foreground
                    text: qsTr("Saved %n images to %1", "", win.savedCount).arg(win.savedFolder)
                }
                Button {
                    flat: true
                    text: qsTr("Open Folder")
                    onClicked: { backend.openFolder(win.savedFolder); savedToast.opacity = 0 }
                }
            }
        }

        // Every shortcut, on ?.
        Rectangle {
            anchors.fill: parent
            visible: win.helpVisible
            color: Qt.alpha(theme.background, 0.88)

            TapHandler { onTapped: win.helpVisible = false }

            Rectangle {
                anchors.centerIn: parent
                width: helpGrid.implicitWidth + 64
                height: helpGrid.implicitHeight + 96
                radius: theme.radius
                color: theme.surface
                border.color: theme.selection

                Label {
                    id: helpTitle
                    anchors.top: parent.top
                    anchors.topMargin: 24
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Keyboard Shortcuts")
                    font.pixelSize: 18
                    color: theme.foreground
                }

                ColumnLayout {
                    id: helpGrid
                    anchors.top: helpTitle.bottom
                    anchors.topMargin: 20
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 10

                    Repeater {
                        model: [
                            ["A", qsTr("Add images")],
                            ["W", qsTr("Type a new width")],
                            ["Enter", qsTr("Apply the width")],
                            ["R", qsTr("Back to the original size")],
                            ["Space", qsTr("Save to a folder")],
                            ["Ctrl+A", qsTr("Select all")],
                            ["Esc", qsTr("Select none")],
                            ["← → ↑ ↓", qsTr("Move; with Shift, extend the selection")],
                            ["Ctrl+Click", qsTr("Add to or take from the selection")],
                            ["Delete", qsTr("Remove from the list")],
                            ["Ctrl+Z", qsTr("Undo")],
                            ["Ctrl+Shift+Z", qsTr("Redo")],
                            ["?", qsTr("Show or hide this help")],
                            ["Q", qsTr("Quit")]
                        ]
                        delegate: Item {
                            required property var modelData
                            implicitWidth: keyLabel.width + 28 + action.implicitWidth
                            implicitHeight: Math.max(keyLabel.height, action.implicitHeight)

                            Rectangle {
                                id: keyLabel
                                width: 120
                                height: keyText.implicitHeight + 8
                                radius: Math.min(4, theme.radius)
                                color: theme.selection
                                Label {
                                    id: keyText
                                    anchors.centerIn: parent
                                    text: modelData[0]
                                    color: theme.accent
                                    font.family: "monospace"
                                }
                            }
                            Label {
                                id: action
                                anchors.left: keyLabel.right
                                anchors.leftMargin: 28
                                anchors.verticalCenter: keyLabel.verticalCenter
                                text: modelData[1]
                                color: theme.foreground
                            }
                        }
                    }
                }
            }
        }
    }

    Dialog {
        id: quitDialog
        anchors.centerIn: parent
        modal: true
        width: Math.min(460, win.width - 64)
        title: qsTr("Quit without saving?")
        Material.theme: theme.dark ? Material.Dark : Material.Light
        Material.roundedScale: theme.radius
        background: Rectangle {
            radius: theme.radius
            color: theme.surface
            border.color: theme.selection
        }
        Overlay.modal: Rectangle { color: Qt.alpha(theme.background, 0.7) }

        Label {
            width: parent.width
            text: qsTr("%n resized images haven't been saved yet.", "", backend.changedCount)
            wrapMode: Text.Wrap
        }
        footer: DialogButtonBox {
            Button {
                text: qsTr("Cancel")
                DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            }
            Button {
                text: qsTr("Quit")
                highlighted: true
                DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            }
        }
        onAccepted: { win.quitConfirmed = true; Qt.quit() }
    }

    Component.onCompleted: grid.forceActiveFocus()
}
