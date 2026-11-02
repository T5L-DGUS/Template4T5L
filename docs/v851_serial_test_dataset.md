# V851 UART4 TLV / Wi-Fi / BOOT OTA 串口测试集

应用固件不再使用 PB-03F 或 JSON。UART4 固定为 115200 波特，接收 TLV、Wi-Fi，以及用于转交 BOOT 的首个 OTA 信息帧：

- TLV：`AA 55 | length BE | 35/36/37/38/39 | segments`，`length = 1 + 所有段字节数`（包含命令字），总长 `4 + length`，最大 2048 字节。`0x37` 按方向复用：T5L → V851 为环境状态加九类执行器的全量 Snapshot，V851 → T5L 为 `0x6C` BootstrapResult。`0x39` 为 T5L → V851 恢复出厂通知。
- Wi-Fi：`AA 55 | len BE | C0/C1/C5 | payload`，总长 `4 + len`。
- BOOT 交接：应用仅接受完整合法的首个 `AB CD 04` 文件信息帧（最大 280 字节），持久化 `0x0020 = 5A A5 5A A5` 后软复位，不发送 05。完整 4124 字节 OTA 数据帧由 BOOT 的 UART4 接收。

TLV 十六进制向量位于 `tests/serial_bridge_vectors.json`，包括 Power、Door、Light、Climate、Filter、Firmware、Storage、FactoryDevice 和 FactoryRejection。`tlv_codec.h` 中 Climate、Firmware 和 FactoryRejection 注释行的长度字段与实际字段字节数不一致；测试向量按正式格式重新计算了这三组长度。

常用模拟器命令：

恢复出厂 `0x39/0x6E` 和版本显示 `0x35/0x09` 的字段、时机及完整帧参见 [恢复出厂与版本协议](t5l_factory_reset_firmware.md)。

```powershell
python tools/t5l_serial_simulator.py decode "AA 55 00 0F 35 03 00 0C 01 00 01 01 02 00 01 55 03 00 01 00"
python tools/t5l_serial_simulator.py wifi-scan 0
python tools/t5l_serial_simulator.py wifi-connect TestNet 1234
python tools/t5l_serial_simulator.py monitor --port COM8 --baud 115200
```

边界和恢复测试由 `python -m unittest discover -s tests -v` 执行，覆盖 2048 字节 TLV、04 交接校验、UART4 溢出恢复、粘包、噪声、截断、畸形长度以及坏帧后的重新同步。BOOT 仓库另有 4124 字节 OTA 协议测试。

控制与状态 VP 分离：V851 下发的执行器控制写入 `0x5100–0x5124` 命令槽；T5L 的 `0x35` 增量和 `0x37` Snapshot 从环境状态 `0x3010` 及执行器实际状态 `0x301A–0x3043` 读取。环境段 `0x05` 上报实时温度和湿度，执行器段为 `0x61–0x69`。启动后的第一次 500ms 扫描只建立状态基线，不触发增量；服务器重复下发但实际状态未变化时也不触发增量。


