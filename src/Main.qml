import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: win
    width: 780
    height: 680
    minimumWidth: 660
    minimumHeight: 620
    visible: true
    title: qsTr("Blackout")
    color: (backend.active || backend.pixelRefresherActive) ? "#000000" : "#0b0b0e"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    property bool helpVisible: false
    readonly property bool overlayLocked: backend.active || backend.pixelRefresherActive

    Shortcut {
        sequence: "?"
        enabled: !win.overlayLocked && !backend.recordingCombo
        context: Qt.ApplicationShortcut
        onActivated: {
            backend.resetActivity()
            win.helpVisible = !win.helpVisible
        }
    }
    Shortcut {
        sequence: "Escape"
        enabled: !backend.active && (win.helpVisible || backend.recordingCombo || backend.pixelRefresherActive)
        context: Qt.ApplicationShortcut
        onActivated: {
            if (backend.pixelRefresherActive)
                backend.stopPixelRefresher()
            else if (backend.recordingCombo)
                backend.cancelRecordingCombo()
            else
                win.helpVisible = false
        }
    }
    Shortcut {
        sequence: "Q"
        enabled: !win.overlayLocked && !backend.recordingCombo
        context: Qt.ApplicationShortcut
        onActivated: Qt.quit()
    }
    Shortcut {
        sequence: "Space"
        enabled: !backend.active && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.primaryAction()
    }
    Shortcut {
        sequences: [StandardKey.Undo]
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.undo()
    }
    Shortcut {
        sequences: [StandardKey.Redo]
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.redo()
    }
    Shortcut {
        sequence: "R"
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.startRecordingCombo()
    }
    Shortcut {
        sequence: "Backspace"
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.resetUnlockSequence()
    }
    Shortcut {
        sequence: "A"
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.toggleAutoBlackout()
    }
    Shortcut {
        sequence: "-"
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.adjustAutoBlackoutMinutes(-1)
    }
    Shortcut {
        sequences: ["+", "="]
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.adjustAutoBlackoutMinutes(1)
    }
    Shortcut {
        sequence: "I"
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.toggleStatusIndicator()
    }
    Shortcut {
        sequence: "B"
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.toggleZeroBrightnessOnBlackout()
    }
    Shortcut {
        sequence: "D"
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.toggleSilenceNotificationsOnBlackout()
    }
    Shortcut {
        sequence: "N"
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.toggleNightLight()
    }
    Shortcut {
        sequence: "P"
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.startPixelRefresher()
    }
    Shortcut {
        sequence: "1"
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.setBrightnessPreset(20)
    }
    Shortcut {
        sequence: "2"
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.setBrightnessPreset(40)
    }
    Shortcut {
        sequence: "3"
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.setBrightnessPreset(60)
    }
    Shortcut {
        sequence: "4"
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.setBrightnessPreset(80)
    }
    Shortcut {
        sequence: "5"
        enabled: !win.overlayLocked && !backend.recordingCombo && !win.helpVisible
        context: Qt.ApplicationShortcut
        onActivated: backend.setBrightnessPreset(100)
    }

    // Reusable Curtain Content (rendered inside both the Wayland LayerOverlay
    // window and the main window fallback so nothing can ever uncover the screen)
    Component {
        id: curtainComponent

        Rectangle {
            id: curtainRoot
            anchors.fill: parent
            color: backend.pixelRefresherActive ? backend.pixelRefresherColor : "#000000"
            focus: true

            Keys.onPressed: function(event) {
                backend.handleKeyPress(event.key, event.modifiers)
                event.accepted = true
            }

            // Rolling white wash bar during Phase 0 of the OLED Pixel Refresher
            Rectangle {
                id: washBar
                visible: backend.pixelRefresherActive && backend.pixelRefresherWashPhase
                width: Math.max(120, parent.width * 0.14)
                height: parent.height
                y: 0
                color: "#f0f0f0"

                NumberAnimation on x {
                    running: washBar.visible
                    loops: Animation.Infinite
                    from: -washBar.width
                    to: curtainRoot.width
                    duration: 2200
                }
            }

            // Swallow all pointer clicks and keep keyboard focus locked
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.AllButtons
                hoverEnabled: true
                cursorShape: Qt.BlankCursor
                onPressed: function(mouse) {
                    curtainRoot.forceActiveFocus()
                    mouse.accepted = true
                }
            }

            // Subtle pixel-shifted OLED status badge (moves every 30s, or flashes briefly on wrong key)
            Rectangle {
                id: statusBadge
                visible: backend.pixelRefresherActive || backend.showStatusIndicator || backend.wrongKeyFlash
                width: badgeRow.implicitWidth + 24
                height: badgeRow.implicitHeight + 14
                radius: Math.min(4, theme.radius)
                color: (backend.wrongKeyFlash || backend.pixelRefresherActive) ? "#0c0c0f" : "#050506"
                border.width: 1
                border.color: (backend.wrongKeyFlash || backend.pixelRefresherActive) ? "#2c2c34" : "#121216"

                x: Math.max(24, Math.min(parent.width - width - 24, (parent.width - width) * backend.indicatorX))
                y: Math.max(24, Math.min(parent.height - height - 24, (parent.height - height) * backend.indicatorY))

                RowLayout {
                    id: badgeRow
                    anchors.centerIn: parent
                    spacing: 10

                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: (backend.wrongKeyFlash || backend.pixelRefresherActive) ? theme.accent : "#2a2a32"
                    }

                    Label {
                        text: {
                            void(translator.language)
                            if (backend.pixelRefresherActive) {
                                return qsTr("Pixel Refresher • %1s • Esc to stop")
                                    .arg(backend.pixelRefresherSecondsLeft.toLocaleString(Qt.locale(), 'f', 0))
                            }
                            if (backend.wrongKeyFlash)
                                return qsTr("Locked • Press %1 to unlock").arg(backend.unlockSequence)
                            return qsTr("%1 • Press %2 to unlock")
                                .arg(backend.activeDurationText)
                                .arg(backend.unlockSequence)
                        }
                        font.pixelSize: 12
                        font.family: "monospace"
                        color: (backend.wrongKeyFlash || backend.pixelRefresherActive) ? "#a0a0ac" : "#2c2c34"
                    }
                }
            }
        }
    }

    // Dedicated Wayland zwlr_layer_shell_v1 LayerOverlay Window:
    // Configured in main.cpp with LayerOverlay, ExclusiveZone(-1), and
    // KeyboardInteractivityExclusive so Hyprland can NEVER tile, shrink, or
    // un-fullscreen it, and it sits above the top bar and notifications.
    Window {
        id: curtainWin
        objectName: "curtainWindow"
        visible: false
        width: Screen.width > 0 ? Screen.width : 1920
        height: Screen.height > 0 ? Screen.height : 1200
        color: "#000000"
        flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

        Loader {
            id: curtainWinLoader
            anchors.fill: parent
            active: win.overlayLocked
            sourceComponent: curtainComponent
            onLoaded: {
                if (item)
                    item.forceActiveFocus()
            }
        }
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        id: keyCatcher
        anchors.fill: parent
        focus: true
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        Keys.onPressed: function(event) {
            if (win.overlayLocked || backend.recordingCombo) {
                backend.handleKeyPress(event.key, event.modifiers)
                event.accepted = true
            } else {
                backend.resetActivity()
            }
        }

        Connections {
            target: backend
            function onActiveChanged() {
                if (curtainWinLoader.item)
                    curtainWinLoader.item.forceActiveFocus()
                else
                    keyCatcher.forceActiveFocus()
            }
            function onPixelRefresherChanged() {
                if (curtainWinLoader.item)
                    curtainWinLoader.item.forceActiveFocus()
                else
                    keyCatcher.forceActiveFocus()
            }
            function onRecordingComboChanged() {
                keyCatcher.forceActiveFocus()
            }
        }

        HoverHandler {
            onPointChanged: {
                if (!win.overlayLocked)
                    backend.resetActivity()
            }
        }

        // Apple HIG System Settings Layout (Inset Grouped Table & Control Center Hero)
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 28
            spacing: 14
            visible: !win.overlayLocked

            // Missing tool / device banner (shown only when a dependency is missing)
            Rectangle {
                visible: backend.statusWarning.length > 0
                Layout.fillWidth: true
                Layout.preferredHeight: warningLabel.implicitHeight + 18
                radius: theme.radius
                color: "#2a1618"
                border.width: 1
                border.color: "#ff453a"

                Label {
                    id: warningLabel
                    anchors.fill: parent
                    anchors.margins: 9
                    text: backend.statusWarning
                    font.pixelSize: 12
                    color: "#ffdad6"
                    wrapMode: Text.WordWrap
                }
            }

            // Top Toolbar Header (Apple HIG Navigation Bar style)
            RowLayout {
                Layout.fillWidth: true
                spacing: 14

                // App Glyph
                Rectangle {
                    width: 40
                    height: 40
                    radius: theme.radius
                    color: "#151519"
                    border.width: 1
                    border.color: "#2c2c34"

                    Rectangle {
                        anchors.centerIn: parent
                        width: 18
                        height: 18
                        radius: 9
                        color: "#000000"
                        border.width: 2
                        border.color: theme.accent
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Label {
                        text: qsTr("Blackout")
                        font.pixelSize: 20
                        font.weight: Font.Bold
                        color: "#f2f2f7"
                    }
                    Label {
                        text: qsTr("OLED Display Care & Lock")
                        font.pixelSize: 12
                        color: "#8e8e93"
                    }
                }

                // Secondary Action Pill: Pixel Refresher (P)
                Rectangle {
                    Layout.preferredHeight: 34
                    Layout.preferredWidth: refreshRow.implicitWidth + 20
                    radius: theme.radius
                    color: refreshHover.hovered ? "#222229" : "#151519"
                    border.width: 1
                    border.color: "#2c2c34"

                    RowLayout {
                        id: refreshRow
                        anchors.centerIn: parent
                        spacing: 8

                        Rectangle {
                            width: 7
                            height: 7
                            radius: 4
                            color: theme.accent
                        }
                        Label {
                            text: qsTr("Pixel Refresher")
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            color: "#f2f2f7"
                        }
                        Rectangle {
                            width: 18
                            height: 18
                            radius: Math.min(4, theme.radius)
                            color: "#0b0b0e"
                            border.width: 1
                            border.color: "#2c2c34"
                            Label {
                                anchors.centerIn: parent
                                text: "P"
                                font.pixelSize: 10
                                font.family: "monospace"
                                font.weight: Font.Bold
                                color: "#8e8e93"
                            }
                        }
                    }
                    HoverHandler { id: refreshHover }
                    TapHandler { onTapped: backend.startPixelRefresher() }
                }

                // Help Button (?)
                Rectangle {
                    width: 34
                    height: 34
                    radius: theme.radius
                    color: helpHover.hovered ? "#222229" : "#151519"
                    border.width: 1
                    border.color: "#2c2c34"

                    Label {
                        anchors.centerIn: parent
                        text: "?"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        color: "#f2f2f7"
                    }
                    HoverHandler { id: helpHover }
                    TapHandler { onTapped: win.helpVisible = !win.helpVisible }
                }
            }

            // Hero Card (Apple Control Center Module aesthetic)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 102
                radius: theme.radius
                color: "#151519"
                border.width: 1
                border.color: "#26262f"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 18
                    spacing: 20

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Locks a pure black Wayland overlay above the bar and notifications while background tasks keep running.")
                            font.pixelSize: 13
                            color: "#e5e5ea"
                            wrapMode: Text.WordWrap
                        }

                        RowLayout {
                            spacing: 16
                            RowLayout {
                                spacing: 6
                                Rectangle { width: 6; height: 6; radius: 3; color: theme.accent }
                                Label {
                                    text: qsTr("LayerOverlay Lock")
                                    font.pixelSize: 11
                                    color: "#8e8e93"
                                }
                            }
                            RowLayout {
                                spacing: 6
                                Rectangle { width: 6; height: 6; radius: 3; color: backend.zeroBrightnessOnBlackout ? theme.accent : "#3a3a44" }
                                Label {
                                    text: qsTr("0% Hardware Backlight")
                                    font.pixelSize: 11
                                    color: backend.zeroBrightnessOnBlackout ? "#8e8e93" : "#636366"
                                }
                            }
                            RowLayout {
                                spacing: 6
                                Rectangle { width: 6; height: 6; radius: 3; color: backend.silenceNotificationsOnBlackout ? theme.accent : "#3a3a44" }
                                Label {
                                    text: qsTr("Notifications Silenced")
                                    font.pixelSize: 11
                                    color: backend.silenceNotificationsOnBlackout ? "#8e8e93" : "#636366"
                                }
                            }
                        }
                    }

                    Rectangle {
                        id: startBtn
                        Layout.preferredWidth: Math.max(182, startBtnRow.implicitWidth + 32)
                        Layout.preferredHeight: 46
                        radius: theme.radius
                        color: theme.accent
                        opacity: startHover.hovered ? 0.92 : 1.0

                        RowLayout {
                            id: startBtnRow
                            anchors.centerIn: parent
                            spacing: 10

                            Label {
                                text: qsTr("Start Blackout")
                                font.pixelSize: 14
                                font.weight: Font.Bold
                                color: theme.accentForeground
                            }

                            Rectangle {
                                Layout.preferredWidth: spaceLabel.implicitWidth + 14
                                Layout.preferredHeight: 22
                                radius: Math.min(4, theme.radius)
                                color: Qt.rgba(0, 0, 0, 0.2)

                                Label {
                                    id: spaceLabel
                                    anchors.centerIn: parent
                                    text: "Space"
                                    font.pixelSize: 11
                                    font.family: "monospace"
                                    font.weight: Font.DemiBold
                                    color: theme.accentForeground
                                }
                            }
                        }

                        HoverHandler { id: startHover }
                        TapHandler { onTapped: backend.startBlackout() }
                    }
                }
            }

            // Apple HIG Inset Grouped Section 1: Lock & Screen Protection (5 full-width rows)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 292
                radius: theme.radius
                color: "#151519"
                border.width: 1
                border.color: backend.recordingCombo ? theme.accent : "#26262f"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    anchors.topMargin: 4
                    anchors.bottomMargin: 4
                    spacing: 0

                    // Row 1: Unlock Key Combination
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        spacing: 14

                        Rectangle {
                            width: 28
                            height: 28
                            radius: Math.min(4, theme.radius)
                            color: "#202028"
                            border.width: 1
                            border.color: "#2e2e38"
                            Label {
                                anchors.centerIn: parent
                                text: "⌘"
                                font.pixelSize: 13
                                font.weight: Font.Bold
                                color: theme.accent
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label {
                                text: qsTr("Unlock Shortcut")
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: "#f2f2f7"
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Only this key combination will exit the black screen")
                                font.pixelSize: 11
                                color: "#8e8e93"
                                elide: Text.ElideRight
                            }
                        }

                        Rectangle {
                            Layout.preferredHeight: 30
                            Layout.preferredWidth: comboLabel.implicitWidth + 20
                            radius: Math.min(4, theme.radius)
                            color: backend.recordingCombo ? Qt.rgba(1, 1, 1, 0.08) : "#0b0b0e"
                            border.width: 1
                            border.color: backend.recordingCombo ? theme.accent : "#2c2c34"

                            Label {
                                id: comboLabel
                                anchors.centerIn: parent
                                text: backend.recordingCombo ? qsTr("Press new key combo…") : backend.unlockSequence
                                font.pixelSize: 12
                                font.family: "monospace"
                                font.weight: Font.DemiBold
                                color: backend.recordingCombo ? theme.accent : "#f2f2f7"
                            }
                        }

                        Rectangle {
                            Layout.preferredHeight: 30
                            Layout.preferredWidth: recordBtnLabel.implicitWidth + 20
                            radius: Math.min(4, theme.radius)
                            color: recordHover.hovered ? "#262630" : "#1e1e26"
                            border.width: 1
                            border.color: "#2e2e38"

                            Label {
                                id: recordBtnLabel
                                anchors.centerIn: parent
                                text: backend.recordingCombo ? qsTr("Cancel") : qsTr("Change")
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                color: "#f2f2f7"
                            }
                            HoverHandler { id: recordHover }
                            TapHandler {
                                onTapped: {
                                    if (backend.recordingCombo)
                                        backend.cancelRecordingCombo()
                                    else
                                        backend.startRecordingCombo()
                                }
                            }
                        }

                        Rectangle {
                            visible: backend.unlockSequence !== "Ctrl+Shift+U" && !backend.recordingCombo
                            Layout.preferredHeight: 30
                            Layout.preferredWidth: resetBtnLabel.implicitWidth + 16
                            radius: Math.min(4, theme.radius)
                            color: resetHover.hovered ? "#262630" : "#1e1e26"
                            border.width: 1
                            border.color: "#2e2e38"

                            Label {
                                id: resetBtnLabel
                                anchors.centerIn: parent
                                text: qsTr("Reset")
                                font.pixelSize: 12
                                color: "#8e8e93"
                            }
                            HoverHandler { id: resetHover }
                            TapHandler { onTapped: backend.resetUnlockSequence() }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.leftMargin: 42; height: 1; color: "#22222a" }

                    // Row 2: 0% Hardware Backlight
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        spacing: 14

                        Rectangle {
                            width: 28
                            height: 28
                            radius: Math.min(4, theme.radius)
                            color: "#202028"
                            border.width: 1
                            border.color: "#2e2e38"
                            Label {
                                anchors.centerIn: parent
                                text: "☀"
                                font.pixelSize: 13
                                color: theme.accent
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label {
                                text: qsTr("0% Hardware Backlight")
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: "#f2f2f7"
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Drop panel drive to 0% while locked and restore on unlock")
                                font.pixelSize: 11
                                color: "#8e8e93"
                                elide: Text.ElideRight
                            }
                        }

                        Switch {
                            checked: backend.zeroBrightnessOnBlackout
                            onToggled: backend.toggleZeroBrightnessOnBlackout()
                        }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.leftMargin: 42; height: 1; color: "#22222a" }

                    // Row 3: Silence Notifications
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        spacing: 14

                        Rectangle {
                            width: 28
                            height: 28
                            radius: Math.min(4, theme.radius)
                            color: "#202028"
                            border.width: 1
                            border.color: "#2e2e38"
                            Label {
                                anchors.centerIn: parent
                                text: "☾"
                                font.pixelSize: 13
                                color: theme.accent
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label {
                                text: qsTr("Silence Notifications")
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: "#f2f2f7"
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Enable Do-Not-Disturb while locked so toasts never wake the OLED panel")
                                font.pixelSize: 11
                                color: "#8e8e93"
                                elide: Text.ElideRight
                            }
                        }

                        Switch {
                            checked: backend.silenceNotificationsOnBlackout
                            onToggled: backend.toggleSilenceNotificationsOnBlackout()
                        }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.leftMargin: 42; height: 1; color: "#22222a" }

                    // Row 4: Auto-Blackout Timer
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        spacing: 14

                        Rectangle {
                            width: 28
                            height: 28
                            radius: Math.min(4, theme.radius)
                            color: "#202028"
                            border.width: 1
                            border.color: "#2e2e38"
                            Label {
                                anchors.centerIn: parent
                                text: "◷"
                                font.pixelSize: 13
                                color: theme.accent
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            RowLayout {
                                spacing: 8
                                Label {
                                    text: qsTr("Auto-Blackout Timer")
                                    font.pixelSize: 13
                                    font.weight: Font.Medium
                                    color: "#f2f2f7"
                                }
                                Rectangle {
                                    visible: backend.autoBlackout
                                    width: countdownLabel.implicitWidth + 10
                                    height: 18
                                    radius: Math.min(4, theme.radius)
                                    color: "#1f1f27"
                                    border.width: 1
                                    border.color: theme.accent

                                    Label {
                                        id: countdownLabel
                                        anchors.centerIn: parent
                                        readonly property int mins: Math.floor(backend.autoBlackoutRemainingSeconds / 60)
                                        readonly property int secs: backend.autoBlackoutRemainingSeconds % 60
                                        text: {
                                            void(translator.language)
                                            return qsTr("In %1:%2")
                                                .arg(mins.toLocaleString(Qt.locale(), 'f', 0))
                                                .arg(secs < 10 ? "0" + secs.toLocaleString(Qt.locale(), 'f', 0) : secs.toLocaleString(Qt.locale(), 'f', 0))
                                        }
                                        font.pixelSize: 10
                                        font.weight: Font.DemiBold
                                        color: theme.accent
                                    }
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: qsTr("Automatically lock the black screen after inactivity")
                                font.pixelSize: 11
                                color: "#8e8e93"
                                elide: Text.ElideRight
                            }
                        }

                        // Apple-style Stepper
                        RowLayout {
                            spacing: 4
                            Rectangle {
                                width: 26
                                height: 26
                                radius: Math.min(4, theme.radius)
                                color: minusHover.hovered ? "#262630" : "#1e1e26"
                                border.width: 1
                                border.color: "#2e2e38"
                                Label {
                                    anchors.centerIn: parent
                                    text: "−"
                                    font.pixelSize: 13
                                    font.weight: Font.Bold
                                    color: "#f2f2f7"
                                }
                                HoverHandler { id: minusHover }
                                TapHandler { onTapped: backend.adjustAutoBlackoutMinutes(-1) }
                            }

                            Rectangle {
                                Layout.preferredWidth: Math.max(62, minsLabel.implicitWidth + 16)
                                height: 26
                                radius: Math.min(4, theme.radius)
                                color: "#0b0b0e"
                                border.width: 1
                                border.color: "#2c2c34"
                                Label {
                                    id: minsLabel
                                    anchors.centerIn: parent
                                    text: {
                                        void(translator.language)
                                        return qsTr("%n min", "", backend.autoBlackoutMinutes)
                                    }
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    color: "#f2f2f7"
                                }
                            }

                            Rectangle {
                                width: 26
                                height: 26
                                radius: Math.min(4, theme.radius)
                                color: plusHover.hovered ? "#262630" : "#1e1e26"
                                border.width: 1
                                border.color: "#2e2e38"
                                Label {
                                    anchors.centerIn: parent
                                    text: "+"
                                    font.pixelSize: 13
                                    font.weight: Font.Bold
                                    color: "#f2f2f7"
                                }
                                HoverHandler { id: plusHover }
                                TapHandler { onTapped: backend.adjustAutoBlackoutMinutes(1) }
                            }
                        }

                        Switch {
                            checked: backend.autoBlackout
                            onToggled: backend.toggleAutoBlackout()
                        }
                    }

                    Rectangle { Layout.fillWidth: true; Layout.leftMargin: 42; height: 1; color: "#22222a" }

                    // Row 5: Pixel-Shifted Status
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 56
                        spacing: 14

                        Rectangle {
                            width: 28
                            height: 28
                            radius: Math.min(4, theme.radius)
                            color: "#202028"
                            border.width: 1
                            border.color: "#2e2e38"
                            Label {
                                anchors.centerIn: parent
                                text: "❖"
                                font.pixelSize: 12
                                color: theme.accent
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Label {
                                text: qsTr("Pixel-Shifted Status")
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: "#f2f2f7"
                            }
                            Label {
                                Layout.fillWidth: true
                                text: backend.showStatusIndicator
                                      ? qsTr("Show a dim status badge that shifts position every 30 seconds")
                                      : qsTr("100% pure black pixels (zero light emission)")
                                font.pixelSize: 11
                                color: "#8e8e93"
                                elide: Text.ElideRight
                            }
                        }

                        Switch {
                            checked: backend.showStatusIndicator
                            onToggled: backend.toggleStatusIndicator()
                        }
                    }
                }
            }

            // Apple HIG Inset Grouped Section 2: Display Luminance & Night Light (Segmented Control)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 68
                radius: theme.radius
                color: "#151519"
                border.width: 1
                border.color: "#26262f"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 14

                    Rectangle {
                        width: 28
                        height: 28
                        radius: Math.min(4, theme.radius)
                        color: "#202028"
                        border.width: 1
                        border.color: "#2e2e38"
                        Label {
                            anchors.centerIn: parent
                            text: "◐"
                            font.pixelSize: 13
                            color: theme.accent
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1

                        Label {
                            text: qsTr("OLED Luminance & Night Light")
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            color: "#f2f2f7"
                        }
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Cap brightness & warm blue sub-pixels")
                            font.pixelSize: 11
                            color: "#8e8e93"
                            elide: Text.ElideRight
                        }
                    }

                    // Apple HIG Segmented Control for Brightness Presets
                    Rectangle {
                        Layout.preferredHeight: 30
                        Layout.preferredWidth: 236
                        radius: Math.min(4, theme.radius)
                        color: "#0b0b0e"
                        border.width: 1
                        border.color: "#2c2c34"

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 2
                            spacing: 2

                            Repeater {
                                model: [20, 40, 60, 80, 100]
                                delegate: Rectangle {
                                    readonly property bool selected: Math.abs(backend.brightnessPercent - modelData) <= 8
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: Math.min(4, theme.radius)
                                    color: selected ? theme.accent : (segHover.hovered ? "#1c1c24" : "transparent")

                                    Label {
                                        anchors.centerIn: parent
                                        text: {
                                            void(translator.language)
                                            return qsTr("%1%").arg(modelData.toLocaleString(Qt.locale(), 'f', 0))
                                        }
                                        font.pixelSize: 11
                                        font.weight: selected ? Font.Bold : Font.Medium
                                        color: selected ? theme.accentForeground : "#e5e5ea"
                                    }
                                    HoverHandler { id: segHover }
                                    TapHandler { onTapped: backend.setBrightnessPreset(modelData) }
                                }
                            }
                        }
                    }

                    Rectangle {
                        width: 1
                        Layout.fillHeight: true
                        Layout.topMargin: 14
                        Layout.bottomMargin: 14
                        color: "#22222a"
                    }

                    RowLayout {
                        spacing: 8
                        Label {
                            text: qsTr("Night Light")
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            color: "#f2f2f7"
                        }
                        Switch {
                            checked: backend.nightLightEnabled
                            onToggled: backend.toggleNightLight()
                        }
                    }
                }
            }

            Item { Layout.fillHeight: true }
        }

        // Keyboard Shortcuts Overlay (?)
        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(0, 0, 0, 0.78)
            visible: win.helpVisible && !win.overlayLocked
            z: 50

            TapHandler { onTapped: win.helpVisible = false }

            Rectangle {
                width: Math.min(parent.width - 64, 520)
                height: helpCol.implicitHeight + 40
                anchors.centerIn: parent
                radius: theme.radius
                color: "#151519"
                border.width: 1
                border.color: theme.accent

                ColumnLayout {
                    id: helpCol
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Keyboard Shortcuts")
                            font.pixelSize: 16
                            font.weight: Font.Bold
                            color: "#f2f2f7"
                        }
                        Rectangle {
                            width: closeLabel.implicitWidth + 16
                            height: 26
                            radius: Math.min(4, theme.radius)
                            color: "#24242c"
                            Label {
                                id: closeLabel
                                anchors.centerIn: parent
                                text: qsTr("Close")
                                font.pixelSize: 12
                                color: "#f2f2f7"
                            }
                            TapHandler { onTapped: win.helpVisible = false }
                        }
                    }

                    Repeater {
                        model: [
                            { key: "Space", label: qsTr("Start Blackout") },
                            { key: backend.unlockSequence, label: qsTr("Unlock Shortcut") },
                            { key: "R", label: qsTr("Change unlock combo") },
                            { key: "Backspace", label: qsTr("Reset unlock combo") },
                            { key: "B", label: qsTr("Toggle 0% hardware backlight") },
                            { key: "D", label: qsTr("Toggle notification silencing") },
                            { key: "A", label: qsTr("Toggle auto-blackout") },
                            { key: "- / +", label: qsTr("Adjust timer minutes") },
                            { key: "I", label: qsTr("Toggle status badge") },
                            { key: "1 – 5", label: qsTr("Set OLED brightness (20%–100%)") },
                            { key: "N", label: qsTr("Toggle Night Light") },
                            { key: "P", label: qsTr("Run OLED Pixel Refresher") },
                            { key: "Ctrl+Z / Ctrl+Shift+Z", label: qsTr("Undo / Redo setting change") },
                            { key: "?", label: qsTr("Show shortcuts") },
                            { key: "Q", label: qsTr("Quit") }
                        ]
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Label {
                                Layout.fillWidth: true
                                text: modelData.label
                                font.pixelSize: 12
                                color: "#e5e5ea"
                            }

                            Rectangle {
                                width: keyText.implicitWidth + 14
                                height: 22
                                radius: Math.min(4, theme.radius)
                                color: "#0b0b0e"
                                border.width: 1
                                border.color: "#2c2c34"

                                Label {
                                    id: keyText
                                    anchors.centerIn: parent
                                    text: modelData.key
                                    font.pixelSize: 11
                                    font.family: "monospace"
                                    font.weight: Font.DemiBold
                                    color: theme.accent
                                }
                            }
                        }
                    }
                }
            }
        }

        // Fallback in-window curtain (active alongside the LayerOverlay window)
        Loader {
            anchors.fill: parent
            z: 100
            active: win.overlayLocked
            sourceComponent: curtainComponent
        }
    }
}
