// Mac-inspired Windows connection page: a device/status card on the left and
// the related voice/bridge settings on the right. Platform behavior remains
// Windows-native and all state-changing operations stay on SettingsController.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import OvbRc003Settings 1.0

Item {
    id: root
    property var tokens

    function diagnosticStatus(checkId) {
        var results = DiagnosticsController.checkResults
        for (var i = 0; i < results.length; i++) {
            if (results[i].checkId === checkId) {
                return results[i].status
            }
        }
        return ""
    }

    readonly property bool rc003Detected:
        diagnosticStatus("raw_input") === "pass"
        && diagnosticStatus("ble_candidate") === "pass"

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        ColumnLayout {
            width: root.width - tokens.spacingLarge * 2
            x: tokens.spacingLarge
            y: tokens.spacingMedium
            spacing: tokens.spacingMedium

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: tokens.spacingMedium
                rowSpacing: tokens.spacingMedium

                Rectangle {
                    Layout.preferredWidth: 250
                    Layout.fillHeight: true
                    implicitHeight: Math.max(520, deviceColumn.implicitHeight + tokens.spacingLarge * 2)
                    radius: tokens.cornerRadiusLarge
                    color: tokens.surface
                    border.color: tokens.border
                    border.width: 1

                    ColumnLayout {
                        id: deviceColumn
                        anchors.fill: parent
                        anchors.margins: tokens.spacingLarge
                        spacing: tokens.spacingSmall

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("当前设备")
                            font.pixelSize: tokens.fontSizeTitle
                            font.bold: true
                            color: tokens.textPrimary
                        }

                        ComboBox {
                            id: deviceCombo
                            objectName: "deviceCombo"
                            Layout.fillWidth: true
                            model: SettingsController.deviceOptions
                            currentIndex: SettingsController.selectedDeviceIndex
                            displayText: SettingsController.isRc003Device
                                ? qsTr("小米蓝牙遥控器 2 Pro")
                                : (SettingsController.isDjiMic2Device
                                    ? qsTr("DJI Mic 2") : currentText)
                            onActivated: SettingsController.selectedDeviceIndex = index
                            enabled: SettingsController.deviceCatalogAvailable
                            Accessible.name: qsTr("当前设备")
                        }

                        Label {
                            visible: !SettingsController.deviceCatalogAvailable
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: SettingsController.deviceCatalogErrorText
                            color: tokens.errorColor
                            font.pixelSize: tokens.fontSizeSmall
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 250

                            Image {
                                anchors.fill: parent
                                anchors.margins: tokens.spacingSmall
                                fillMode: Image.PreserveAspectFit
                                source: SettingsController.isRc003Device && SettingsController.photoAvailable
                                    ? SettingsController.photoSource : ""
                                visible: source.toString().length > 0
                                smooth: true
                                asynchronous: true
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: !SettingsController.isRc003Device
                                    || !SettingsController.photoAvailable
                                text: SettingsController.isDjiMic2Device
                                    ? qsTr("DJI Mic 2") : qsTr("遥控器图片不可用")
                                color: tokens.textSecondary
                                font.pixelSize: tokens.fontSizeTitle
                                font.bold: true
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            radius: tokens.cornerRadiusSmall
                            color: tokens.selectionBackground
                            implicitHeight: deviceStatusColumn.implicitHeight + tokens.spacingMedium * 2

                            ColumnLayout {
                                id: deviceStatusColumn
                                anchors.fill: parent
                                anchors.margins: tokens.spacingMedium
                                spacing: tokens.spacingTiny

                                Label {
                                    text: SettingsController.isRc003Device
                                        ? qsTr("设备状态") : qsTr("输入状态")
                                    color: tokens.accent
                                    font.pixelSize: tokens.fontSizeSmall
                                    font.bold: true
                                }
                                Label {
                                    Layout.fillWidth: true
                                    wrapMode: Text.WordWrap
                                    text: SettingsController.isRc003Device
                                        ? (root.rc003Detected
                                            ? qsTr("已检测到 RC003：Raw Input 与蓝牙均已就绪；按键与语音仍需真机验收。")
                                            : qsTr("尚未确认 RC003 的 Raw Input 与蓝牙状态；请前往“检查”页重新检测。"))
                                        : SettingsController.djiMicStatusText
                                    color: tokens.textPrimary
                                    font.pixelSize: tokens.fontSizeSmall
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: tokens.spacingSmall
                            Button {
                                Layout.fillWidth: true
                                text: qsTr("恢复默认")
                                onClicked: SettingsController.restoreDefaults()
                            }
                            Button {
                                id: deviceSaveButton
                                objectName: "deviceSaveButton"
                                Layout.fillWidth: true
                                text: qsTr("保存选择")
                                highlighted: true
                                onClicked: SettingsController.saveSettings()
                            }
                        }
                    }
                }

                Rectangle {
                    visible: SettingsController.isRc003Device
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    implicitHeight: Math.max(520, settingsColumn.implicitHeight + tokens.spacingLarge * 2)
                    radius: tokens.cornerRadiusLarge
                    color: tokens.surface
                    border.color: tokens.border
                    border.width: 1

                    ColumnLayout {
                        id: settingsColumn
                        anchors.fill: parent
                        anchors.margins: tokens.spacingLarge
                        spacing: tokens.spacingSmall

                        Label {
                            text: qsTr("语音设置")
                            font.pixelSize: tokens.fontSizeTitle
                            font.bold: true
                            color: tokens.textPrimary
                        }

                        Label {
                            text: qsTr("语音输出设备")
                            color: tokens.textPrimary
                            font.pixelSize: tokens.fontSizeBody
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("语音只写入当前选中的设备；设备缺失时普通按键仍可使用。")
                            color: tokens.textSecondary
                            font.pixelSize: tokens.fontSizeSmall
                        }
                        ComboBox {
                            id: endpointCombo
                            objectName: "endpointCombo"
                            Layout.fillWidth: true
                            model: SettingsController.endpointOptions
                            currentIndex: SettingsController.selectedEndpointIndex
                            onActivated: SettingsController.selectedEndpointIndex = index
                            Accessible.name: qsTr("语音输出设备")
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: tokens.spacingSmall
                            Layout.bottomMargin: tokens.spacingSmall
                            implicitHeight: 1
                            color: tokens.border
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: tokens.spacingMedium
                            rowSpacing: tokens.spacingSmall

                            Label {
                                text: qsTr("语音组合键")
                                color: tokens.textPrimary
                                font.pixelSize: tokens.fontSizeBody
                            }
                            TextField {
                                id: hotkeyField
                                objectName: "hotkeyField"
                                Layout.fillWidth: true
                                text: SettingsController.hotkeyText
                                selectByMouse: true
                                onEditingFinished: SettingsController.hotkeyText = text
                                Accessible.name: qsTr("语音热键")
                            }

                            Label {
                                text: qsTr("触发方式")
                                color: tokens.textPrimary
                                font.pixelSize: tokens.fontSizeBody
                            }
                            ComboBox {
                                id: triggerModeCombo
                                Layout.fillWidth: true
                                model: SettingsController.triggerModeOptions
                                currentIndex: SettingsController.triggerModeIndex
                                onActivated: SettingsController.triggerModeIndex = index
                                Accessible.name: qsTr("语音触发方式")
                            }
                        }

                        Connections {
                            target: SettingsController
                            function onHotkeyTextChanged() {
                                hotkeyField.text = SettingsController.hotkeyText
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("免按住使用右 Alt+空格；长按使用右 Alt。切换触发方式会同步更新组合键。")
                            color: tokens.textSecondary
                            font.pixelSize: tokens.fontSizeSmall
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.topMargin: tokens.spacingSmall
                            Layout.bottomMargin: tokens.spacingSmall
                            implicitHeight: 1
                            color: tokens.border
                        }

                        Label {
                            text: qsTr("桥接状态")
                            font.pixelSize: tokens.fontSizeTitle
                            font.bold: true
                            color: tokens.textPrimary
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: SettingsController.launchStatusText
                            color: tokens.textSecondary
                            font.pixelSize: tokens.fontSizeSmall
                        }

                        RowLayout {
                            spacing: tokens.spacingSmall
                            Button {
                                id: saveAndLaunchButton
                                text: qsTr("保存并启动桥接")
                                highlighted: true
                                onClicked: SettingsController.saveAndLaunch()
                                KeyNavigation.tab: openLogButton
                            }
                            Button {
                                id: openLogButton
                                objectName: "openLogButton"
                                text: qsTr("打开日志目录")
                                onClicked: SettingsController.openLogLocation()
                            }
                        }

                        Item { Layout.fillHeight: true }

                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            visible: text.length > 0
                            text: SettingsController.errorMessage
                            color: tokens.errorColor
                            font.pixelSize: tokens.fontSizeSmall
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            visible: text.length > 0 && SettingsController.errorMessage.length === 0
                            text: SettingsController.statusMessage
                            color: tokens.successColor
                            font.pixelSize: tokens.fontSizeSmall
                        }
                    }
                }

                Rectangle {
                    visible: SettingsController.isDjiMic2Device
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    implicitHeight: Math.max(520, djiColumn.implicitHeight + tokens.spacingLarge * 2)
                    radius: tokens.cornerRadiusLarge
                    color: tokens.surface
                    border.color: tokens.border
                    border.width: 1

                    ColumnLayout {
                        id: djiColumn
                        anchors.fill: parent
                        anchors.margins: tokens.spacingLarge
                        spacing: tokens.spacingMedium

                        Label {
                            text: qsTr("DJI Mic 2 录音输入")
                            font.pixelSize: tokens.fontSizeTitle
                            font.bold: true
                            color: tokens.textPrimary
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: SettingsController.djiMicStatusText
                            color: tokens.textSecondary
                            font.pixelSize: tokens.fontSizeBody
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: qsTr("DJI Mic 2 使用 Windows 系统录音输入，不启动 RC003 桥接，也不会修改默认输入设备。")
                            color: tokens.textSecondary
                            font.pixelSize: tokens.fontSizeSmall
                        }
                        RowLayout {
                            spacing: tokens.spacingSmall
                            Button {
                                text: qsTr("重新检测")
                                onClicked: SettingsController.refreshDjiMicStatus()
                            }
                            Button {
                                text: qsTr("打开声音输入设置")
                                highlighted: true
                                onClicked: SettingsController.openSoundSettings()
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }
                }
            }

            Item { Layout.preferredHeight: tokens.spacingMedium }
        }
    }
}
