from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


class V851ControlDgusTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.header = (REPO_ROOT / "modules/v851_control_info.h").read_text(encoding="utf-8")
        cls.source = (REPO_ROOT / "modules/v851_control_info.c").read_text(encoding="utf-8")
        cls.protocol = (REPO_ROOT / "modules/v851_protocol.c").read_text(encoding="utf-8")
        cls.protocol_h = (REPO_ROOT / "modules/v851_protocol.h").read_text(encoding="utf-8")
        cls.tlv_app = (REPO_ROOT / "modules/v851_tlv_app.c").read_text(encoding="utf-8")

    def test_command_and_report_address_layouts_are_separate(self) -> None:
        command_addresses = {
            "EXHAUST": "0x5100UL",
            "INLET_FAN": "0x5104UL",
            "UVB": "0x510CUL",
            "HUMIDIFIER": "0x5110UL",
            "LIGHT": "0x5114UL",
            "CLIMATE": "0x5118UL",
            "PLASMA": "0x5120UL",
            "ANION": "0x5124UL",
        }
        report_addresses = {
            "ENVIRONMENT": "0x3010UL",
            "EXHAUST": "0x301AUL",
            "LIGHT": "0x301FUL",
            "UVB": "0x3024UL",
            "ANION": "0x3029UL",
            "PLASMA": "0x302EUL",
            "CLIMATE": "0x3033UL",
            "HUMIDIFIER": "0x3038UL",
            "INLET_FAN": "0x303DUL",
            "FILTER": "0x3042UL",
        }
        for name, address in command_addresses.items():
            self.assertIn(f"V851_CONTROL_COMMAND_{name}_ADDR", self.header)
            self.assertIn(address, self.header)
        for name, address in report_addresses.items():
            self.assertIn(f"V851_CONTROL_REPORT_{name}_ADDR", self.header)
            self.assertIn(address, self.header)
        self.assertNotIn("V851_CONTROL_DGUS_", self.header)

    def test_all_nine_actuator_fields_are_mapped_by_protocol_type(self) -> None:
        self.assertIn("V851ControlInfoMapWritableField", self.source)
        self.assertIn("V851ControlInfoWriteHours", self.source)
        self.assertIn("V851ControlInfoHoursToCode", self.source)
        for struct_name in (
            "EXHAUST", "LIGHT", "UVB", "ANION", "PLASMA", "CLIMATE",
            "HUMIDIFIER", "INLET_FAN", "FILTER",
        ):
            self.assertIn(f"V851_TLV_STRUCT_{struct_name}", self.source)
        for tag_name in (
            "EXH_INTERVAL_HOURS", "EXH_RUNNING", "LIGHT_RUNNING",
            "UVB_LEVEL", "UVB_DAILY_HOURS", "UVB_RUNNING", "ANION_RUNNING",
            "PLASMA_RUNNING", "CLIMATE_CTRL_STATUS", "CLIMATE_RUNNING",
            "HUMI_INTERVAL_HOURS", "HUMI_RUNNING_HOURS", "HUMI_RUNNING",
            "HUMI_LIQUID_STATUS", "INLET_RUNNING", "FILTER_LIFE_PERCENT",
            "FILTER_NEED_REPLACE",
        ):
            self.assertIn(f"V851_TLV_TAG_{tag_name}", self.source)
        for obsolete in (
            "target_humidity",
            "duration_minutes",
            "color_temperature",
            "core_json",
        ):
            self.assertNotIn(obsolete, self.source)

    def test_whole_frame_validation_precedes_any_apply(self) -> None:
        first_pass = self.protocol.index("First pass: reject the complete frame")
        second_pass = self.protocol.index("Second pass: the complete frame")
        apply = self.protocol.index("V851ControlInfoApplySegment", second_pass)
        self.assertLess(first_pass, second_pass)
        self.assertLess(second_pass, apply)
        self.assertNotIn("V851ControlInfoApplySegment", self.protocol[first_pass:second_pass])

    def test_command_6c_is_accepted_without_7800_debug_write(self) -> None:
        self.assertIn(
            "#define V851_TLV_CMD_BOOTSTRAP_RESULT_COMPAT     0x6CU",
            self.protocol_h,
        )
        self.assertIn(
            "command != V851_TLV_CMD_BOOTSTRAP_RESULT_COMPAT",
            self.protocol,
        )
        self.assertIn(
            "if(command == V851_TLV_CMD_BOOTSTRAP_RESULT_COMPAT)",
            self.protocol,
        )
        self.assertNotIn("V851ProtocolWriteBootstrapDebugFrame", self.protocol)
        self.assertNotIn("V851_BOOTSTRAP_DEBUG_VP_ADDR", self.protocol)

    def test_remote_write_targets_command_and_optimistic_report_state(self) -> None:
        self.assertIn("uint8_t V851ControlInfoApplySegment", self.header)
        self.assertIn("uint8_t V851ControlInfoApplySegment", self.source)
        self.assertIn("return changed;", self.source)
        apply_start = self.source.index("uint8_t V851ControlInfoApplySegment")
        apply_end = self.source.index("uint16_t V851ControlInfoBuildFields", apply_start)
        apply = self.source[apply_start:apply_end]
        self.assertIn("V851ControlInfoCommandAddress(struct_type)", apply)
        self.assertIn("V851ControlInfoCommandWords(struct_type)", apply)
        self.assertIn("write_dgus_vp(address, record, command_words)", apply)
        self.assertIn("T5lStcSyncMappedControl", apply)
        self.assertNotIn("T5lStcSyncLocalMappedControl", apply)
        self.assertIn("T5L_STC_MAPPED_FIELD_ENABLED", apply)
        self.assertIn("T5L_STC_MAPPED_FIELD_SECONDARY", apply)
        self.assertIn("T5L_STC_MAPPED_FIELD_TERTIARY", apply)
        self.assertNotIn("V851ControlInfoReportAddress", apply)
        self.assertNotIn("v851_control_report_shadow", apply)

    def test_remote_enabled_dispatches_all_eight_stc_switches_after_sync(self) -> None:
        helper_start = self.source.index("static void V851ControlInfoApplySwitch")
        helper_end = self.source.index("uint8_t V851ControlInfoApplySegment", helper_start)
        helper = self.source[helper_start:helper_end]
        for struct_name, on_call, off_call in (
            ("EXHAUST", "Exhaust_On()", "Exhaust_Off(0U)"),
            ("LIGHT", "Light_On(setting)", "Light_Off()"),
            ("UVB", "UVB_On(setting)", "UVB_Off(0U)"),
            ("ANION", "Dev_Switch_Ctrl(&G_Device_Ctrl.Anion.enable, ANION_CMDWORD, 1U)", "Dev_Switch_Ctrl(&G_Device_Ctrl.Anion.enable, ANION_CMDWORD, 0U)"),
            ("PLASMA", "Dev_Switch_Ctrl(&G_Device_Ctrl.Plasma.enable, PLASMA_CMDWORD, 1U)", "Dev_Switch_Ctrl(&G_Device_Ctrl.Plasma.enable, PLASMA_CMDWORD, 0U)"),
            ("CLIMATE", "Heater_On((int16_t)setting)", "Heater_Off(0U)"),
            ("HUMIDIFIER", "Humidifier_On()", "Humidifier_Off(0U)"),
            ("INLET_FAN", "InWind_On(setting)", "InWind_Off()"),
        ):
            with self.subTest(struct_name=struct_name):
                self.assertIn(f"V851_TLV_STRUCT_{struct_name}", helper)
                self.assertIn(on_call, helper)
                self.assertIn(off_call, helper)
        self.assertNotIn("V851_TLV_STRUCT_FILTER", helper)

        apply_start = self.source.index("uint8_t V851ControlInfoApplySegment")
        apply_end = self.source.index("uint16_t V851ControlInfoBuildFields", apply_start)
        apply = self.source[apply_start:apply_end]
        write = apply.index("write_dgus_vp(address, record, command_words)")
        sync = apply.index("T5lStcSyncMappedControl", write)
        enabled_guard = apply.index("mapped_mask & T5L_STC_MAPPED_FIELD_ENABLED", sync)
        dispatch = apply.index("V851ControlInfoApplySwitch", enabled_guard)
        self.assertLess(write, sync)
        self.assertLess(sync, enabled_guard)
        self.assertLess(enabled_guard, dispatch)

    def test_full_and_incremental_reporting_read_actual_state_vps(self) -> None:
        build_start = self.source.index("uint16_t V851ControlInfoBuildFields")
        scan_start = self.source.index("uint16_t V851ControlInfoScanChanged")
        build = self.source[build_start:scan_start]
        scan = self.source[scan_start:]
        self.assertIn("V851ControlInfoReportAddress(struct_type)", build)
        self.assertIn("V851ControlInfoReportAddress(struct_type)", scan)
        self.assertNotIn("V851ControlInfoCommandAddress", build)
        self.assertNotIn("V851ControlInfoCommandAddress", scan)

    def test_environment_temperature_and_humidity_use_shared_report_path(self) -> None:
        self.assertIn("V851_CONTROL_ENVIRONMENT_INDEX", self.header)
        self.assertIn("V851ControlInfoReportStructType", self.header)
        self.assertIn("V851_TLV_STRUCT_ENVIRONMENT", self.source)
        self.assertIn("V851_TLV_TAG_TEMPERATURE", self.source)
        self.assertIn("G_Device_Ctrl.environment.temperaturex10", self.source)
        self.assertIn("V851_TLV_TAG_HUMIDITY", self.source)
        self.assertIn(
            "read_dgus_vp(V851_CONTROL_REPORT_ENVIRONMENT_ADDR, record, 3U)",
            self.source,
        )
        self.assertIn("V851ControlInfoReadBe16(&record[4])", self.source)
        self.assertIn("V851TlvWriteBinary64Uint16", self.source)
        self.assertIn("V851ControlInfoWriteTenths", self.source)

    def test_first_scan_silently_establishes_baseline_without_init_api(self) -> None:
        self.assertNotIn("V851ControlInfoInit", self.header)
        self.assertNotIn("V851ControlInfoInit", self.source)
        self.assertNotIn("V851ControlInfoInit", self.protocol)
        self.assertIn("v851_control_report_shadow_valid_mask", self.source)
        scan_start = self.source.index("uint16_t V851ControlInfoScanChanged")
        scan = self.source[scan_start:]
        baseline = scan.index("v851_control_report_shadow_valid_mask & valid_bit")
        changed = scan.index("changed_mask |= valid_bit")
        self.assertLess(baseline, changed)

    def test_binary64_uses_words_not_native_double(self) -> None:
        self.assertIn("uint32_t high_word", self.protocol)
        self.assertIn("uint32_t low_word", self.protocol)
        self.assertIn("exponent_bits", self.protocol)
        self.assertNotIn("double", self.protocol)

    def test_all_documented_types_and_tags_are_public(self) -> None:
        for value in range(0x01, 0x0B):
            self.assertIn(f"0x{value:02X}U", self.protocol_h)
        for value in range(0x61, 0x6E):
            self.assertIn(f"0x{value:02X}U", self.protocol_h)
        for name in (
            "POWER_MODE", "LAST_OPEN_TIME", "CO2", "EXH_INTERVAL_HOURS",
            "LIGHT_RUNNING", "UVB_DAILY_HOURS", "ANION_RUNNING",
            "PLASMA_RUNNING", "CLIMATE_CTRL_STATUS", "HUMI_LIQUID_STATUS",
            "INLET_RUNNING", "FILTER_NEED_REPLACE", "LAST_ERROR_CODE",
            "DISP_SCREEN_MODE", "LOCAL_PASSWORD", "CONFIG_VERSION",
            "STORAGE_FREE", "FACTORY_FW_VERSION", "FACTORY_REJECT_REASON",
            "BOOT_DEVICE_SN", "BOOT_BLE_ID", "BOOT_API_ENDPOINT",
            "BOOT_BIND_STATUS", "BOOT_QR_URL", "EA_IS_ALARM", "EA_CODE",
            "EA_LEVEL", "EA_RECOVERED", "EA_PAYLOAD",
        ):
            self.assertIn(f"V851_TLV_TAG_{name}", self.protocol_h)

    def test_single_direct_project_hook(self) -> None:
        self.assertEqual(self.protocol.count("V851TlvApplicationSegment(command, &segment)"), 1)
        self.assertNotIn("V851ControlHandler", self.protocol)
        self.assertEqual(self.tlv_app.count("void V851TlvApplicationSegment("), 1)

    def test_bootstrap_cache_revalidates_before_writing(self) -> None:
        validate = self.tlv_app.index("Validate again locally")
        clear = self.tlv_app.index("memset(&v851_bootstrap_result")
        write = self.tlv_app.index("V851BootstrapWriteQr();", clear)
        valid = self.tlv_app.index("v851_bootstrap_result_valid = 1U")
        self.assertLess(validate, clear)
        self.assertLess(clear, write)
        self.assertLess(write, valid)


if __name__ == "__main__":
    unittest.main()
