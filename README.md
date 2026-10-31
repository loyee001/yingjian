# 等你来 · 桌面求助按钮硬件

孩子按下桌面按钮，ESP32 通过 BLE 通知安卓 APP，让家长及时看到求助。日常使用蓝牙，不需要手机和按钮连接同一个 Wi-Fi。

当前接线：**Signal → D23 / GPIO23；VCC → 3V3；GND → GND**。当前固件按下为 HIGH，使用 INPUT_PULLDOWN、35 毫秒消抖。旧 GPIO4 接法不适用于这版。

## 固件下载

| 文件 | 用途 |
| --- | --- |
| [deng_ni_lai_button.ino](deng_ni_lai_button.ino) | 主固件，日常使用 |
| [button_d23_diagnostic.ino](button_d23_diagnostic.ino) | GPIO23 诊断固件，增加串口电平和手机订阅日志 |
| [hardware-source.zip](hardware-source.zip) | 完整硬件源包，保留 firmware / diagnostics / docs / verification 目录及全部 12 项原文件 |

只选一个固件烧录。单独下载 INO 时，将文件放到与文件名同名的文件夹里，再用 Arduino IDE 打开；下载 ZIP 则解压后打开 `hardware/firmware/deng_ni_lai_button/deng_ni_lai_button.ino`。

## 接线、编译与备料

- [三根线怎么接](WIRING.md)
- [Arduino IDE / CLI 编译和烧录](BUILD.md)
- [完整材料清单](BOM.md) · [纯文本备料清单](完整备料清单.txt)
- [BLE 协议](PROTOCOL.md)
- [诊断固件说明](diagnostics-README.md)
- [已验证范围](VERIFICATION.md)

在 Arduino IDE 中安装 **esp32 by Espressif Systems 3.3.2**。当前经典 ESP32 样机采用 ESP32 Dev Module、4 MB Flash、DIO、40 MHz、无 PSRAM、上传速度 115200。其他开发板要按实物选择，不能默认都有 GPIO23。

ESP32 烧录并通电后，手机打开配套 APP，允许蓝牙附近设备和悬浮窗权限，开始作业陪伴并连接 **DengNiLai Button**。显示就绪后切到其他 APP，松开再按按钮测试提醒。按住只计一次，松开后再按才计下一次；未连接手机时的按下不会积压补发。

## 验证和限制

2026-10-31 当前 GPIO23 诊断版本已实际烧录并通过四段 Flash 哈希校验；串口记录有 GPIO23 0/1 变化、手机订阅和 50 条按键通知。用户在红米手机的抖音界面确认“按下会弹，连续按几次会覆盖整屏”。发布主固件另外独立编译通过，本次打包未烧录、未操作手机。验证范围不扩展到所有手机、全部 APP 或横屏。

当前没有扬声器音频功能。可选音频材料和后续所需改动见材料清单。仓库不包含本机工具链、完整原始调试日志、串口/手机设备标识或配对码。

## 文件完整性

- [manifest.json](manifest.json)：本网页全部发布文件的大小与 SHA-256，不含清单自身。
- [hardware-source-manifest.json](hardware-source-manifest.json)：完整源包内的原始文件清单。
- [hardware-source-package.json](hardware-source-package.json)：ZIP 大小、SHA-256 及其中 12 项文件摘要。
- [hardware-source-audit.json](hardware-source-audit.json)：源码、接线和独立编译的一致性核查。

完整源包 SHA-256：`81f02d6479a99ce8925c9cfeb115db926fd627730ae413f1843555859a1795af`。
