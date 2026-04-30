import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: root

    width: 1080
    height: 720
    minimumWidth: 980
    minimumHeight: 660
    visible: true
    title: qsTr("MyCross 控制台")
    color: "#070a0f"

    property bool profileRailCollapsed: width < 980
    property bool saveSuccess: false
    property string infoMessage: qsTr("就绪")

    property color bg0: "#070a0f"
    property color bg1: "#101623"
    property color panel0: "#111827"
    property color panel1: "#172033"
    property color panel2: "#1b2740"
    property color stroke: "#26364d"
    property color strokeHover: "#3d5878"
    property color accent: "#22d3ee"
    property color lime: "#a3e635"
    property color danger: "#f43f5e"
    property color warning: "#f59e0b"
    property color titleText: "#f4f8ff"
    property color text: "#c8d3e3"
    property color muted: "#7f91aa"
    property color disabled: "#4b5b70"

    readonly property color previewColor: Qt.rgba(redInput.value / 255, greenInput.value / 255, blueInput.value / 255, 1)
    readonly property string previewHex: {
        const r = redInput.value.toString(16).toUpperCase().padStart(2, "0")
        const g = greenInput.value.toString(16).toUpperCase().padStart(2, "0")
        const b = blueInput.value.toString(16).toUpperCase().padStart(2, "0")
        return "#" + r + g + b
    }

    function loadConfig(cfg) {
        xInput.value = cfg.x ?? -1
        yInput.value = cfg.y ?? -1
        windowSizeInput.value = cfg.window_size ?? 40
        crossHalfInput.value = cfg.cross_half ?? 10
        lineWidthInput.value = cfg.line_width ?? 2
        redInput.value = cfg.color_r ?? 0
        greenInput.value = cfg.color_g ?? 255
        blueInput.value = cfg.color_b ?? 0
    }

    function currentConfig() {
        return {
            x: xInput.value,
            y: yInput.value,
            window_size: windowSizeInput.value,
            cross_half: crossHalfInput.value,
            line_width: lineWidthInput.value,
            color_r: redInput.value,
            color_g: greenInput.value,
            color_b: blueInput.value
        }
    }

    function syncProfileSelection() {
        const idx = profileSelect.indexOfValue(crosshair.activeProfile)
        if (idx >= 0) {
            profileSelect.currentIndex = idx
        }
        profileName.text = crosshair.activeProfile
    }

    function notifyError(action, ok) {
        if (!ok && crosshair.lastError.length > 0) {
            infoMessage = qsTr("%1失败：%2").arg(action).arg(crosshair.lastError)
        }
    }

    function applyNow() {
        crosshair.applyConfig(currentConfig())
    }

    Component.onCompleted: {
        loadConfig(crosshair.config)
        syncProfileSelection()
    }

    onClosing: crosshair.quitApp()

    Connections {
        target: crosshair
        function onConfigChanged() { root.loadConfig(crosshair.config) }
        function onActiveProfileChanged() { root.syncProfileSelection() }
        function onProfilesChanged() { root.syncProfileSelection() }
        function onLastErrorChanged() {
            if (crosshair.lastError.length > 0) {
                root.infoMessage = crosshair.lastError
            }
        }
    }

    background: Rectangle {
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: root.bg0 }
            GradientStop { position: 1.0; color: root.bg1 }
        }

        Rectangle {
            width: 520
            height: 520
            radius: width / 2
            x: -200
            y: -160
            color: "#12445a"
            opacity: 0.2
        }

        Rectangle {
            width: 460
            height: 460
            radius: width / 2
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: -140
            anchors.bottomMargin: -140
            color: "#15305f"
            opacity: 0.2
        }

        Item {
            anchors.fill: parent
            opacity: 0.09
            Repeater {
                model: 50
                Rectangle {
                    width: parent.width
                    height: 1
                    y: index * 20
                    color: "#9bb4d6"
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        GlassPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            radius: 18

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Label {
                        text: qsTr("MyCross")
                        color: root.titleText
                        font.pixelSize: 22
                        font.bold: true
                        font.letterSpacing: 0.8
                        Accessible.name: text
                    }
                    Label {
                        text: qsTr("Tactical Glass Console")
                        color: root.muted
                        font.pixelSize: 11
                    }
                }

                StatusPill {
                    text: qsTr("配置 %1").arg(crosshair.activeProfile.length > 0 ? crosshair.activeProfile : "default.ini")
                    tone: "neutral"
                }

                StatusPill {
                    text: qsTr("热键 %1").arg(crosshair.hotkey)
                    tone: "neutral"
                }

                StatusPill {
                    text: crosshair.running ? qsTr("运行中") : qsTr("已停止")
                    tone: crosshair.lastError.length > 0 ? "danger" : (crosshair.running ? "success" : "warning")
                }

                PrimaryActionButton {
                    id: saveButton
                    text: root.saveSuccess ? qsTr("已保存") : qsTr("保存")
                    accentStyle: "primary"
                    Accessible.name: qsTr("保存当前配置")
                    onClicked: {
                        const ok = crosshair.saveProfile(profileName.text, root.currentConfig())
                        root.notifyError(qsTr("保存"), ok)
                        if (ok) {
                            root.saveSuccess = true
                            root.infoMessage = qsTr("配置已保存")
                            saveResetTimer.restart()
                        }
                    }
                }

                PrimaryActionButton {
                    text: crosshair.running ? qsTr("停止准星") : qsTr("启动准星")
                    accentStyle: crosshair.running ? "danger" : "primary"
                    Accessible.name: text
                    onClicked: crosshair.setRunning(!crosshair.running)
                }

                PrimaryActionButton {
                    text: qsTr("退出")
                    accentStyle: "ghostDanger"
                    Accessible.name: text
                    onClicked: crosshair.quitApp()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            GlassPanel {
                id: profileRail
                Layout.preferredWidth: root.profileRailCollapsed ? 64 : 250
                Layout.fillHeight: true
                radius: 18

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            visible: !root.profileRailCollapsed
                            text: qsTr("Profiles")
                            color: root.titleText
                            font.pixelSize: 16
                            font.bold: true
                        }
                        Button {
                            text: root.profileRailCollapsed ? qsTr("展开") : qsTr("收起")
                            Accessible.name: qsTr("折叠配置栏")
                            onClicked: root.profileRailCollapsed = !root.profileRailCollapsed
                        }
                    }

                    ComboBox {
                        id: profileSelect
                        visible: !root.profileRailCollapsed
                        Layout.fillWidth: true
                        model: crosshair.profiles
                        Accessible.name: qsTr("配置列表")
                        onActivated: {
                            if (currentText.length > 0) {
                                const ok = crosshair.loadProfile(currentText)
                                root.notifyError(qsTr("加载"), ok)
                            }
                        }
                    }

                    TextField {
                        id: profileName
                        visible: !root.profileRailCollapsed
                        Layout.fillWidth: true
                        placeholderText: qsTr("配置名，自动补齐 .ini")
                        Accessible.name: qsTr("配置名称输入")
                    }

                    PrimaryActionButton {
                        visible: !root.profileRailCollapsed
                        Layout.fillWidth: true
                        text: qsTr("新建配置")
                        accentStyle: "ghost"
                        enabled: profileName.text.trim().length > 0
                        onClicked: root.notifyError(qsTr("新建"), crosshair.createProfile(profileName.text, root.currentConfig()))
                    }

                    PrimaryActionButton {
                        visible: !root.profileRailCollapsed
                        Layout.fillWidth: true
                        text: qsTr("重命名")
                        accentStyle: "ghost"
                        enabled: profileName.text.trim().length > 0
                        onClicked: root.notifyError(qsTr("重命名"), crosshair.renameProfile(profileSelect.currentText, profileName.text))
                    }

                    GlassPanel {
                        visible: !root.profileRailCollapsed
                        Layout.fillWidth: true
                        Layout.preferredHeight: 100
                        tone: "soft"
                        radius: 14

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            Label {
                                text: qsTr("当前配置")
                                color: root.muted
                                font.pixelSize: 12
                            }
                            Label {
                                text: crosshair.activeProfile.length > 0 ? crosshair.activeProfile : qsTr("default.ini")
                                color: root.titleText
                                font.pixelSize: 16
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Label {
                                text: qsTr("窗口 %1，线宽 %2").arg(windowSizeInput.value).arg(lineWidthInput.value)
                                color: root.muted
                                font.pixelSize: 12
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }

            GlassPanel {
                id: inspectorPanel
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 18

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: inspectorPanel.width - 34
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                text: qsTr("参数面板")
                                color: root.titleText
                                font.pixelSize: 16
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                            Label {
                                text: qsTr("RGB %1/%2/%3  %4").arg(redInput.value).arg(greenInput.value).arg(blueInput.value).arg(root.previewHex)
                                color: root.muted
                                font.family: "Consolas"
                                font.pixelSize: 12
                            }
                        }

                        GroupCard {
                            title: qsTr("Position")
                            ParameterControl { id: xInput; label: qsTr("X 坐标"); hint: qsTr("-1 为居中"); from: -1; to: Math.max(0, root.Screen.width - 1); onChanged: root.applyNow() }
                            ParameterControl { id: yInput; label: qsTr("Y 坐标"); hint: qsTr("-1 为居中"); from: -1; to: Math.max(0, root.Screen.height - 1); onChanged: root.applyNow() }
                        }

                        GroupCard {
                            title: qsTr("Geometry")
                            ParameterControl { id: windowSizeInput; label: qsTr("窗口尺寸"); hint: qsTr("20..800"); from: 20; to: 800; onChanged: root.applyNow() }
                            ParameterControl { id: crossHalfInput; label: qsTr("准星半径"); hint: qsTr("<= 窗口一半"); from: 1; to: Math.max(1, Math.floor(windowSizeInput.value / 2)); onChanged: root.applyNow() }
                            ParameterControl { id: lineWidthInput; label: qsTr("线宽"); hint: qsTr("1..20"); from: 1; to: 20; onChanged: root.applyNow() }
                        }

                        GroupCard {
                            title: qsTr("Color")
                            ColorChannelControl { id: redInput; label: qsTr("R"); fromColor: "#000000"; toColor: "#FF0000"; onChanged: root.applyNow() }
                            ColorChannelControl { id: greenInput; label: qsTr("G"); fromColor: "#000000"; toColor: "#00FF00"; onChanged: root.applyNow() }
                            ColorChannelControl { id: blueInput; label: qsTr("B"); fromColor: "#000000"; toColor: "#0088FF"; onChanged: root.applyNow() }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 44
                                radius: 10
                                color: root.previewColor
                                border.color: "#d9ecff"
                                Label {
                                    anchors.centerIn: parent
                                    text: root.previewHex
                                    color: "#02101b"
                                    font.pixelSize: 13
                                    font.bold: true
                                    font.family: "Consolas"
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            radius: 12
            color: crosshair.lastError.length > 0 ? Qt.rgba(0.6, 0.26, 0.05, 0.55) : Qt.rgba(0.07, 0.1, 0.16, 0.84)
            border.color: crosshair.lastError.length > 0 ? root.warning : root.stroke

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                Label {
                    Layout.fillWidth: true
                    text: crosshair.lastError.length > 0
                        ? qsTr("错误：%1").arg(crosshair.lastError)
                        : qsTr("状态：%1 | 当前配置：%2 | %3")
                            .arg(crosshair.running ? qsTr("运行中") : qsTr("已停止"))
                            .arg(crosshair.activeProfile.length > 0 ? crosshair.activeProfile : "default.ini")
                            .arg(root.infoMessage)
                    color: crosshair.lastError.length > 0 ? "#ffe2b3" : root.muted
                    elide: Text.ElideRight
                    font.pixelSize: 12
                }
            }
        }
    }

    Timer {
        id: saveResetTimer
        interval: 1200
        onTriggered: root.saveSuccess = false
    }

    component GlassPanel: Rectangle {
        property string tone: "default"
        radius: 16
        color: tone === "soft" ? Qt.rgba(0.09, 0.14, 0.22, 0.86) : Qt.rgba(0.09, 0.13, 0.2, 0.92)
        border.color: root.stroke
    }

    component StatusPill: Rectangle {
        property string text: ""
        property string tone: "neutral"
        implicitHeight: 32
        implicitWidth: 96
        radius: 999
        color: tone === "success" ? "#153326" : tone === "danger" ? "#3e1824" : tone === "warning" ? "#3f3119" : "#0e1a2a"
        border.color: tone === "success" ? "#2e8b64" : tone === "danger" ? "#8f3350" : tone === "warning" ? "#8e6e2a" : "#324865"

        Label {
            anchors.centerIn: parent
            text: parent.text
            color: root.text
            font.pixelSize: 12
            font.bold: true
        }
    }

    component PrimaryActionButton: Button {
        id: actionBtn
        property string accentStyle: "primary"
        implicitHeight: 38
        implicitWidth: 92
        hoverEnabled: true
        font.pixelSize: 13

        background: Rectangle {
            radius: 10
            color: {
                if (!actionBtn.enabled) return "#233246"
                if (actionBtn.accentStyle === "danger") return Qt.rgba(0.78, 0.2, 0.34, 0.88)
                if (actionBtn.accentStyle === "ghostDanger") return Qt.rgba(0.24, 0.1, 0.16, 0.9)
                if (actionBtn.accentStyle === "ghost") return Qt.rgba(0.11, 0.17, 0.26, 0.88)
                return Qt.rgba(0.13, 0.83, 0.93, 0.9)
            }
            border.color: actionBtn.hovered ? root.strokeHover : root.stroke
            opacity: actionBtn.down ? 0.82 : 1.0
        }

        contentItem: Label {
            text: actionBtn.text
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: actionBtn.accentStyle === "primary" ? "#03131d" : root.text
            font: actionBtn.font
        }
    }

    component GroupCard: Rectangle {
        id: grp
        default property alias content: contentCol.data
        property string title: ""
        Layout.fillWidth: true
        implicitHeight: groupLayout.implicitHeight + 16
        Layout.preferredHeight: implicitHeight
        radius: 12
        color: "#152236"
        border.color: "#304965"
        border.width: 1

        ColumnLayout {
            id: groupLayout
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8
            Label {
                text: grp.title
                color: root.titleText
                font.pixelSize: 13
                font.bold: true
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: "#2a3f5b"
            }
            ColumnLayout {
                id: contentCol
                Layout.fillWidth: true
                spacing: 7
            }
        }
    }

    component ParameterControl: Rectangle {
        id: p
        property alias value: slider.value
        property alias from: slider.from
        property alias to: slider.to
        property string label: ""
        property string hint: ""
        signal changed()

        Layout.fillWidth: true
        Layout.preferredHeight: 76
        radius: 10
        color: "#1a2a41"
        border.color: "#36516f"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 7
            spacing: 5

            RowLayout {
                Layout.fillWidth: true
                Label { text: p.label; color: root.text; font.pixelSize: 11; font.bold: true }
                Label { text: p.hint; color: root.muted; font.pixelSize: 10 }
                Item { Layout.fillWidth: true }
                SpinBox {
                    id: spin
                    Layout.preferredWidth: 128
                    Layout.preferredHeight: 30
                    from: Math.floor(slider.from)
                    to: Math.floor(slider.to)
                    value: Math.floor(slider.value)
                    editable: true
                    font.family: "Consolas"
                    font.pixelSize: 12
                    Accessible.name: p.label
                    onValueModified: {
                        slider.value = value
                        p.changed()
                    }
                }
            }

            Slider {
                id: slider
                Layout.fillWidth: true
                Layout.preferredHeight: 22
                stepSize: 1
                snapMode: Slider.SnapAlways
                onMoved: {
                    spin.value = Math.round(value)
                    p.changed()
                }
                onValueChanged: spin.to = Math.floor(to)
            }
        }
    }

    component ColorChannelControl: Rectangle {
        id: c
        property alias value: spin.value
        property string label: ""
        property color fromColor: "#000"
        property color toColor: "#fff"
        signal changed()

        Layout.fillWidth: true
        Layout.preferredHeight: 76
        radius: 10
        color: "#1a2a41"
        border.color: "#36516f"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 7
            spacing: 5

            RowLayout {
                Layout.fillWidth: true
                Label { text: c.label; color: root.text; font.pixelSize: 11; font.bold: true }
                Item { Layout.fillWidth: true }
                SpinBox {
                    id: spin
                    Layout.preferredWidth: 128
                    Layout.preferredHeight: 30
                    from: 0
                    to: 255
                    value: 0
                    editable: true
                    font.family: "Consolas"
                    font.pixelSize: 12
                    Accessible.name: qsTr("颜色通道 %1").arg(c.label)
                    onValueModified: {
                        slider.value = value
                        c.changed()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 14
                radius: 8
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: c.fromColor }
                    GradientStop { position: 1.0; color: c.toColor }
                }
                border.width: 1
                border.color: "#3b5676"

                Slider {
                    id: slider
                    anchors.fill: parent
                    anchors.leftMargin: -4
                    anchors.rightMargin: -4
                    from: 0
                    to: 255
                    stepSize: 1
                    value: 0
                    snapMode: Slider.SnapAlways
                    background: Item {}
                    onMoved: {
                        spin.value = Math.round(value)
                        c.changed()
                    }
                }
            }
        }
    }
}
