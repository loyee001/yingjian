# GPIO23 诊断固件

打开 [button_d23_diagnostic.ino](button_d23_diagnostic.ino) 编译烧录（Arduino IDE 需将其放入同名文件夹）。它用于查看 GPIO23 的高低电平、消抖状态和手机是否订阅，串口波特率为 115200。

当前接法是 Signal→D23/GPIO23、VCC→3V3、GND→GND，按下 HIGH。它保持与主固件相同的 BLE 协议和 35 ms 消抖，增加每秒心跳及最多每 100 ms 一条的原始变化日志。实际烧录的当前样机使用此诊断版本，详情见 [VERIFICATION.md](VERIFICATION.md)。

仓库不打包旧 GPIO4 诊断版，避免把历史接法当作当前接法。
