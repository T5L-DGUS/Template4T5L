from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


class V851FactoryAlarmSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.protocol = (REPO_ROOT / "modules/v851_protocol.c").read_text(encoding="utf-8")
        cls.protocol_h = (REPO_ROOT / "modules/v851_protocol.h").read_text(encoding="utf-8")
        cls.stc = (REPO_ROOT / "modules/t5l_stc.c").read_text(encoding="utf-8")
        cls.uart = (REPO_ROOT / "source/uart.c").read_text(encoding="utf-8")
        cls.config_t5l = (REPO_ROOT / "include/T5L/T5LOSConfig.h").read_text(encoding="utf-8")
        cls.config_t5f = (REPO_ROOT / "include/T5F/T5FOSConfig.h").read_text(encoding="utf-8")

    def test_factory_trigger_and_retry_api(self) -> None:
        self.assertIn("void V851ProtocolRequestFactoryReport(void);", self.protocol_h)
        self.assertIn("case 0x30A1:", self.stc)
        self.assertIn("V851ProtocolRequestFactoryReport();", self.stc)
        self.assertIn("v851_factory_report_pending = 1U", self.protocol)
        self.assertIn("if(V851ProtocolQueueFactoryReport() != 0U)", self.protocol)
        self.assertIn("v851_factory_report_pending = 0U", self.protocol)

    def test_factory_defaults_are_configurable_on_t5l_and_t5f(self) -> None:
        defaults = {
            "v851FACTORY_SALES_COUNTRY": '"CN"',
            "v851FACTORY_PRODUCT_KEY": '"MCQX_PET_CABIN"',
            "v851FACTORY_MODEL": '"MCQX-PET-CABIN-V1"',
            "v851FACTORY_HARDWARE_VERSION": '"HW-V2.0"',
            "v851FACTORY_FIRMWARE_VERSION": '"FW-V1.0.0"',
        }
        for config in (self.config_t5l, self.config_t5f):
            for name, value in defaults.items():
                with self.subTest(name=name):
                    self.assertIn(f"#define {name}", config)
                    self.assertIn(value, config)

    def test_factory_and_alarm_segment_restrictions(self) -> None:
        self.assertIn("segments[index].struct_type != V851_TLV_STRUCT_FACTORY_DEVICE", self.protocol)
        self.assertIn("segments[index].struct_type != V851_TLV_STRUCT_EVENT_ALARM", self.protocol)
        self.assertIn("V851_TLV_CMD_EVENT_ALARM                 0x38U", self.protocol_h)
        self.assertIn("V851_TLV_STRUCT_EVENT_ALARM              0x6DU", self.protocol_h)
        self.assertIn("(uint16_t)(segment_bytes + 1U + crc_bytes)", self.protocol)

    def test_alarm_scans_ten_words_every_500ms(self) -> None:
        self.assertIn("V851_ALARM_SCAN_INTERVAL_MS               500UL", self.protocol)
        self.assertIn("V851_ALARM_VP_BASE                        0x3080UL", self.protocol)
        self.assertIn("V851_ALARM_VP_COUNT                       10U", self.protocol)
        self.assertIn("read_dgus_vp(V851_ALARM_VP_BASE", self.protocol)
        self.assertIn("v851_alarm_current", self.protocol)
        self.assertIn("v851_alarm_reported", self.protocol)
        self.assertIn("v851_alarm_next_index", self.protocol)

    def test_all_defined_alarm_codes_and_levels_are_mapped(self) -> None:
        expected = {
            "TEMP_HIGH": "HIGH",
            "TEMP_LOW": "HIGH",
            "TEMP_SENSOR_FAULT": "HIGH",
            "HEATER_FAULT": "HIGH",
            "OVERHEAT_PROTECTION": "CRITICAL",
            "LIQUID_LOW_WARNING": "MEDIUM",
            "LOW_LIQUID": "MEDIUM",
            "NEBULIZER_DRY_BURN": "HIGH",
            "WATER_LEVEL_SENSOR_FAULT": "HIGH",
            "HUMIDITY_SENSOR_FAULT": "HIGH",
            "EXHAUST_FAN_FAULT": "HIGH",
            "INLET_FAN_FAULT": "HIGH",
            "FILTER_LIFE_EXHAUSTED": "MEDIUM",
        }
        for code, level in expected.items():
            with self.subTest(code=code):
                self.assertIn(f'"{code}"', self.protocol)
                self.assertIn(f'"{level}"', self.protocol)

    def test_alarm_fields_omit_optional_payload(self) -> None:
        queue_start = self.protocol.index("static uint8_t V851ProtocolQueueEventAlarm")
        queue_end = self.protocol.index("static uint8_t V851ProtocolServiceAlarm", queue_start)
        queue = self.protocol[queue_start:queue_end]
        for tag in ("EA_IS_ALARM", "EA_CODE", "EA_LEVEL", "EA_RECOVERED"):
            self.assertIn(tag, queue)
        self.assertNotIn("EA_PAYLOAD", queue)

    def test_38_is_not_received_or_used_by_ota(self) -> None:
        self.assertNotIn("V851_TLV_CMD_EVENT_ALARM", self.uart)
        self.assertNotIn("V851_TLV_CMD_OTA_STATUS", self.protocol_h)
        self.assertNotIn("V851ProtocolSendOtaFrame", self.protocol + self.protocol_h)
        self.assertNotIn("OtaCompleteFlag", self.protocol)


if __name__ == "__main__":
    unittest.main()
