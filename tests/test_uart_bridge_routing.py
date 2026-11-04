from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


class UartRoutingTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.config = (REPO_ROOT / "include/T5L/T5LOSConfig.h").read_text(encoding="utf-8")
        cls.config_t5f = (REPO_ROOT / "include/T5F/T5FOSConfig.h").read_text(encoding="utf-8")
        cls.uart = (REPO_ROOT / "source/uart.c").read_text(encoding="utf-8")
        cls.uart_h = (REPO_ROOT / "source/uart.h").read_text(encoding="utf-8")
        cls.main = (REPO_ROOT / "user/main.c").read_text(encoding="utf-8")
        cls.protocol = (REPO_ROOT / "modules/v851_protocol.c").read_text(encoding="utf-8")
        cls.protocol_h = (REPO_ROOT / "modules/v851_protocol.h").read_text(encoding="utf-8")
        cls.project = (REPO_ROOT / "project/T5L51.uvproj").read_text(encoding="utf-8")

    def test_tlv_uart_buffers_have_exact_budget_and_guards(self) -> None:
        for config, baudrate in ((self.config, 921600), (self.config_t5f, 115200)):
            for expected in (
                "#define v851PROTOCOL_ENABLED             1",
                "#define uartUART2_ENABLED               1",
                "#define uartUART2_TXBUF_SIZE         256",
                "#define uartUART2_RXBUF_SIZE         256",
                "#define uartUART_COMMON_FRAME_SIZE     2048",
                "#define uartUART4_RXBUF_SIZE         2049",
                "#define uartUART4_TXBUF_SIZE         2049",
                f"#define uartUART4_BAUDRATE           {baudrate}",
                "uartUART_COMMON_FRAME_SIZE < 2048U",
                "uartUART4_RXBUF_SIZE <= 2048U",
                "uartUART4_TXBUF_SIZE <= 2048U",
            ):
                with self.subTest(config=config[:30], expected=expected):
                    self.assertIn(expected, config)

    def test_main_registers_only_v851_wifi_and_tlv_tasks(self) -> None:
        self.assertIn("V851WifiInit();", self.main)
        self.assertIn("V851ProtocolInit();", self.main)
        self.assertIn("V851_WIFI_TASK_INTERVAL, V851WifiTask", self.main)
        self.assertIn("V851_PROTOCOL_TASK_INTERVAL, V851ProtocolTask", self.main)
        for removed in ("Ota", "Pb03f", "R11", "4G_air780e", "core_json"):
            self.assertNotIn(removed, self.main)

    def test_uart4_dispatches_tlv_wifi_and_dgus(self) -> None:
        self.assertIn("V851 RX length includes the command byte", self.uart)
        self.assertIn("V851ProtocolReceiveFrame(&frame[frame_offset]", self.uart)
        self.assertIn("V851WifiReceiveFrame(&frame[frame_offset]", self.uart)
        self.assertIn("UartStandardDwin8283Protocal(uart, &frame[frame_offset]", self.uart)
        self.assertNotIn("OtaReceive", self.uart)
        self.assertNotIn("V851_OTA_FRAME_MAX", self.protocol_h)
        self.assertNotIn("V851ProtocolSendOtaFrame", self.protocol + self.protocol_h)

    def test_v851_property_capture_uses_command_inclusive_length(self) -> None:
        frame = bytes.fromhex(
            "AA 55 00 10 35 62 00 0C "
            "01 00 01 01 02 00 01 02 03 00 01 01"
        )
        declared = int.from_bytes(frame[2:4], "big")
        self.assertEqual(len(frame), declared + 4)
        self.assertEqual(len(frame), 8 + int.from_bytes(frame[6:8], "big"))
        self.assertIn("len - V851_TLV_RX_LENGTH_BASE_SIZE", self.protocol)

    def test_uart4_overflow_discards_batch_and_recovers(self) -> None:
        self.assertIn("uint8_t RxOverflow:1", self.uart_h)
        self.assertIn("if(next_head == Uart4.RxTail)", self.uart)
        self.assertIn("Uart4.RxOverflow = 1U", self.uart)
        self.assertIn("uart->RxTail = uart->RxHead", self.uart)
        self.assertIn("uart->RxOverflow = 0U", self.uart)

    def test_removed_product_modules_are_physically_absent(self) -> None:
        removed = (
            "source/core_json.c", "source/core_json.h",
            "modules/ota.c", "modules/ota.h",
            "modules/bridge_json.c", "modules/bridge_json.h",
            "modules/pb03f_ble.c", "modules/pb03f_ble.h",
            "modules/v851_control_mock.c", "modules/v851_control_mock.h",
            "modules/4G_air780e.c", "modules/4G_air780e.h",
            "modules/r11_common.c", "modules/r11_common.h",
            "modules/r11_netskinAnalyze.c", "modules/r11_netskinAnalyze.h",
            "modules/r11_n5camera.c", "modules/r11_n5camera.h",
            "modules/r11_advertise.c", "modules/r11_advertise.h",
            "modules/suggestionsGB2312.h",
        )
        for relative in removed:
            with self.subTest(relative=relative):
                self.assertFalse((REPO_ROOT / relative).exists())
                self.assertNotIn(Path(relative).name, self.project)

    def test_keil_project_contains_handoff_and_active_v851_modules(self) -> None:
        for filename in (
            "boot_handoff.c", "boot_handoff.h", "v851_protocol.c",
            "v851_tlv_app.c", "v851_wifi.c", "v851_wifi.h",
        ):
            self.assertIn(filename, self.project)


if __name__ == "__main__":
    unittest.main()
