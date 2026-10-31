# 编译、烧录与串口检查

本仓库网页采用平铺文件。使用下面的完整路径和 CLI 命令前，先下载 [hardware-source.zip](hardware-source.zip)，解压后进入其中的 `hardware` 目录。也可直接下载 [主固件](deng_ni_lai_button.ino) 或 [诊断固件](button_d23_diagnostic.ino)，分别保存到与草图同名的文件夹，再用 Arduino IDE 打开。

## Arduino IDE

1. 安装 Arduino IDE。
2. 在开发板管理器中安装 **esp32 by Espressif Systems 3.3.2**。BLE 库随该开发板包提供，不要额外安装同名第三方 BLE 库。
3. 打开 `firmware/deng_ni_lai_button/deng_ni_lai_button.ino`。
4. 当前样机为经典 ESP32、4 MB Flash；使用与实物相符的开发板。该样机采用 ESP32 Dev Module、4 MB、DIO、40 MHz、无 PSRAM、上传速度 115200。
5. 选择电脑实际识别的 USB 串口，先“验证”，再“上传”。首次连接或自动下载失败时，可能需要按开发板的 BOOT/EN 操作；以该开发板说明为准。
6. 打开 115200 串口监视器，启动后应出现 `DengNiLai Button ready; scan from the Android app`。

该启动行只说明程序执行到 BLE 广播启动之后，不单独证明手机已经连上。

## Arduino CLI

在仓库根目录运行。`PORT` 是占位符，替换为自己的设备端口，不要照抄。

```sh
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.2
arduino-cli board list
arduino-cli compile --fqbn 'esp32:esp32:esp32:UploadSpeed=115200,FlashFreq=40,FlashMode=dio,FlashSize=4M,PartitionScheme=default,PSRAM=disabled' --build-path build/production firmware/deng_ni_lai_button
arduino-cli upload --fqbn 'esp32:esp32:esp32:UploadSpeed=115200,FlashFreq=40,FlashMode=dio,FlashSize=4M,PartitionScheme=default,PSRAM=disabled' --port PORT --input-dir build/production firmware/deng_ni_lai_button
```

## 排查按键

要看每秒电平和手机订阅状态时，改为打开 `diagnostics/button_d23_diagnostic/button_d23_diagnostic.ino`。它与主固件使用相同 GPIO23、有效电平、35 ms 消抖和 BLE 协议；额外输出 GPIO23 的原始值、消抖状态、上次是否已松开、手机连接和订阅状态。

常见输出：

```text
Phone connected; waiting for notification subscription
Phone subscribed: button ready
DIAG GPIO23 change raw=1 stablePressed=0 armed=1 phoneConnected=1 phoneSubscribed=1
PRESS:1234abcd:1
```

`1234abcd` 仅为示例 bootId。若没有订阅手机，按下会输出 `Button pressed; no subscribed phone, not sent`。若电平完全不变，先核对 Signal 是否接 GPIO23、VCC/GND 是否接对、模块在 3.3V 下是否正常，再判断模块或线材问题。

诊断固件会频繁输出串口信息，日常使用可以换回主固件。此仓库未附预编译二进制，避免把某块板的分区、闪存和串口配置直接套到另一块板上。
