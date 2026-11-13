import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// One image in the grid: its thumbnail, name, and old and new size.
Item {
    id: tile

    required property int index
    required property url source
    required property string name
    required property int originalWidth
    required property int originalHeight
    required property int newWidth
    required property int newHeight
    required property bool changed
    required property bool enlarged
    required property bool selected
    property bool current: false

    signal clicked(int index, int modifiers)

    function size(w, h) {
        // Reads translator.language so numbers re-format on a language switch.
        translator.language
        return qsTr("%1 × %2").arg(w.toLocaleString(Qt.locale(), 'f', 0)).arg(h.toLocaleString(Qt.locale(), 'f', 0))
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 6
        radius: theme.radius
        color: tile.selected ? Qt.alpha(theme.accent, 0.16)
             : hover.hovered ? Qt.tint(theme.surface, Qt.alpha(theme.foreground, 0.06)) : theme.surface
        border.width: tile.selected ? 2 : tile.current ? 1 : 0
        border.color: tile.selected ? theme.accent : theme.muted

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Image {
                    id: thumbnail
                    anchors.fill: parent
                    source: tile.source
                    sourceSize: Qt.size(320, 320)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    autoTransform: true
                    cache: false
                }
                BusyIndicator {
                    anchors.centerIn: parent
                    running: thumbnail.status === Image.Loading
                    visible: running
                }
            }

            Label {
                Layout.fillWidth: true
                text: tile.name
                elide: Text.ElideMiddle
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 13
                color: theme.foreground
            }

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 12
                color: theme.muted
                text: tile.size(tile.originalWidth, tile.originalHeight)
                visible: !tile.changed
            }

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 12
                elide: Text.ElideRight
                textFormat: Text.StyledText
                visible: tile.changed
                color: theme.muted
                text: qsTr("%1 → %2").arg(tile.size(tile.originalWidth, tile.originalHeight))
                          .arg("<font color=\"" + (tile.enlarged ? theme.warning : theme.accent) + "\">"
                               + tile.size(tile.newWidth, tile.newHeight) + "</font>")
                ToolTip.visible: tile.enlarged && sizeHover.hovered
                ToolTip.text: qsTr("Larger than the original, so it will look softer.")
                HoverHandler { id: sizeHover }
            }
        }

        HoverHandler { id: hover }
        TapHandler {
            acceptedModifiers: Qt.KeyboardModifierMask
            onTapped: (point, button) => tile.clicked(tile.index, point.modifiers)
        }
    }
}
