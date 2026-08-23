import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import OvbRc003Settings 1.0

Item {
    id: root
    property var tokens
    property int selectedMetric: 0 // 0 = voice duration, 1 = trigger frequency

    function heatColor(level, future) {
        if (future) return Qt.rgba(tokens.textPrimary.r, tokens.textPrimary.g, tokens.textPrimary.b, 0.025)
        if (level <= 0) return tokens.heatmapEmpty
        if (level === 1) return Qt.rgba(tokens.accent.r, tokens.accent.g, tokens.accent.b, 0.24)
        if (level === 2) return Qt.rgba(tokens.accent.r, tokens.accent.g, tokens.accent.b, 0.45)
        if (level === 3) return Qt.rgba(tokens.accent.r, tokens.accent.g, tokens.accent.b, 0.70)
        return tokens.accent
    }

    onVisibleChanged: {
        if (visible) UsageStatisticsController.refresh()
    }

    ScrollView {
        anchors.fill: parent
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: root.width - tokens.spacingLarge * 2
            x: tokens.spacingLarge
            y: tokens.spacingSmall
            spacing: tokens.spacingMedium

            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: qsTr("按天聚合过去一年的遥控器使用情况。")
                    color: tokens.textSecondary
                    font.pixelSize: tokens.fontSizeSmall
                }
                Rectangle {
                    radius: 999
                    color: tokens.successBackground
                    implicitWidth: privacyLabel.implicitWidth + 22
                    implicitHeight: privacyLabel.implicitHeight + 10
                    Label {
                        id: privacyLabel
                        anchors.centerIn: parent
                        text: qsTr("仅保存在本机")
                        color: tokens.successColor
                        font.pixelSize: tokens.fontSizeSmall
                        font.bold: true
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: tokens.spacingMedium
                Repeater {
                    model: [
                        { label: qsTr("今天语音"), value: UsageStatisticsController.todayDuration,
                            note: UsageStatisticsController.todayFrequency },
                        { label: qsTr("最近一年"), value: UsageStatisticsController.yearDuration,
                            note: UsageStatisticsController.yearFrequency },
                        { label: qsTr("活跃天数"), value: UsageStatisticsController.activeDays,
                            note: qsTr("过去 365 天") }
                    ]
                    delegate: Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 108
                        radius: tokens.cornerRadiusLarge
                        color: tokens.surface
                        border.color: tokens.border
                        border.width: 1
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: tokens.spacingMedium
                            spacing: tokens.spacingTiny
                            Label { text: modelData.label; color: tokens.textSecondary; font.pixelSize: tokens.fontSizeSmall }
                            Label { text: modelData.value; color: tokens.textPrimary; font.pixelSize: 24; font.bold: true }
                            Label { text: modelData.note; color: tokens.textSecondary; font.pixelSize: tokens.fontSizeSmall }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 326
                radius: tokens.cornerRadiusLarge
                color: tokens.surface
                border.color: tokens.border
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: tokens.spacingLarge
                    spacing: tokens.spacingMedium

                    RowLayout {
                        Layout.fillWidth: true
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: tokens.spacingTiny
                            Label { text: qsTr("每日使用"); color: tokens.textPrimary; font.pixelSize: tokens.fontSizeTitle; font.bold: true }
                            Label { text: qsTr("颜色越深，当天使用越多"); color: tokens.textSecondary; font.pixelSize: tokens.fontSizeSmall }
                        }
                        Button {
                            text: qsTr("语音时长")
                            checkable: true
                            checked: root.selectedMetric === 0
                            onClicked: root.selectedMetric = 0
                        }
                        Button {
                            text: qsTr("触发次数")
                            checkable: true
                            checked: root.selectedMetric === 1
                            onClicked: root.selectedMetric = 1
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 152

                        Column {
                            x: 0
                            y: 19
                            spacing: 19
                            Label { text: qsTr("周一"); color: tokens.textSecondary; font.pixelSize: 13 }
                            Label { text: qsTr("周三"); color: tokens.textSecondary; font.pixelSize: 13 }
                            Label { text: qsTr("周五"); color: tokens.textSecondary; font.pixelSize: 13 }
                        }

                        Item {
                            id: heatmap
                            x: 42
                            width: parent.width - 42
                            height: parent.height
                            readonly property real pitch: Math.min(13, (width - 1) / 53)
                            readonly property real cellSize: Math.max(7, pitch - 3)

                            Repeater {
                                model: UsageStatisticsController.heatmapCells
                                delegate: Rectangle {
                                    required property var modelData
                                    x: modelData.weekIndex * heatmap.pitch
                                    y: 18 + modelData.dayIndex * heatmap.pitch
                                    width: heatmap.cellSize
                                    height: heatmap.cellSize
                                    radius: 2
                                    color: root.heatColor(
                                        root.selectedMetric === 0 ? modelData.durationLevel : modelData.frequencyLevel,
                                        modelData.isFuture
                                    )
                                    border.color: Qt.rgba(tokens.textPrimary.r, tokens.textPrimary.g, tokens.textPrimary.b, 0.07)
                                    border.width: 1
                                    ToolTip.visible: hover.containsMouse && !modelData.isFuture
                                    ToolTip.text: modelData.date + " · "
                                        + (root.selectedMetric === 0 ? modelData.durationText : modelData.frequencyText)
                                    MouseArea { id: hover; anchors.fill: parent; hoverEnabled: true }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Label { Layout.fillWidth: true; text: UsageStatisticsController.rangeText; color: tokens.textSecondary; font.pixelSize: tokens.fontSizeSmall }
                        Label { text: qsTr("少"); color: tokens.textSecondary; font.pixelSize: 13 }
                        Repeater {
                            model: 5
                            delegate: Rectangle {
                                width: 11; height: 11; radius: 2
                                color: root.heatColor(index, false)
                                border.color: tokens.border
                            }
                        }
                        Label { text: qsTr("多"); color: tokens.textSecondary; font.pixelSize: 13 }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.preferredHeight: implicitHeight
                wrapMode: Text.WordWrap
                text: qsTr("隐私说明：只记录每天累计的遥控器按键次数、语音会话次数和语音时长；不保存文字、音频、应用名称或设备标识。")
                color: tokens.textSecondary
                font.pixelSize: tokens.fontSizeSmall
            }
            Item { Layout.preferredHeight: tokens.spacingMedium }
        }
    }
}
