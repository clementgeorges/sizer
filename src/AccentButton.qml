import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The main action, filled with the theme's accent and labelled in whichever
// of black or white reads on it, in light and dark themes alike.
Button {
    id: control

    // Off, it's an ordinary flat button (Add Images once the list has images).
    property bool filled: true

    flat: !filled
    Material.roundedScale: theme.radius

    contentItem: Label {
        text: control.text
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: !control.enabled ? theme.muted
             : control.filled ? theme.accentForeground : theme.foreground
    }

    background: Rectangle {
        implicitWidth: 64
        implicitHeight: 40
        y: (control.height - height) / 2
        height: 40
        radius: theme.radius
        visible: control.filled || control.hovered || control.down
        color: {
            if (!control.filled)
                return Qt.alpha(theme.foreground, control.down ? 0.16 : 0.08)
            if (!control.enabled)
                return Qt.alpha(theme.foreground, 0.12)
            if (control.down)
                return Qt.darker(theme.accent, 1.15)
            return control.hovered ? Qt.lighter(theme.accent, 1.08) : theme.accent
        }
    }
}
