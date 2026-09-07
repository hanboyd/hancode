# Remote Mic 产品设计系统

本设计系统将既有的 Remote Mic macOS 设置界面转化为 RC003 Windows 客户端设计，不模仿 macOS 窗口外观或仅限该平台的行为。

## 参考来源

- 只读的 Mac 实现：`Sources/RemoteMic/SettingsView.swift`
- Mac 设计约定：`design-qa.md`
- Mac 参考截图：连接、按键映射、权限/隐私
- 共用产品资产：`Resources/RC003-remote-photo.png`
- Windows 实现：`apps/windows/rc003/src/ovb_rc003/qml/*.qml`

## 产品表达规则

- 使用窄导航栏，持续展示产品分区。
- 每页以一个大标题开头，不设置冗余副标题。
- 内容采用克制的分层表面与具语义的蓝色选中状态。
- 设备标识、实时状态和主要恢复操作应当保持在一起。
- 中文界面文本不得小于 12 pt。
- Windows 设置链接与 VB-CABLE 保持 Windows 原生体验；不复制 macOS 控件和窗口外观。

## 索引

- `tokens/colors_and_type.css`：权威视觉令牌。
- `brand/style-notes.md`：布局、交互与适配规则。
- `brand/voice-and-tone.md`：产品文案规则。
- `assets/icons/RemoteMic-AppIcon.svg`：权威麦克风产品标识。
- `ui-kit-windows/`：代表性组件与组合界面。
