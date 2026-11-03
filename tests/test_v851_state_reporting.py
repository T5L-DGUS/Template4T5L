from __future__ import annotations

import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


class V851StateAndWifiTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.protocol = (REPO_ROOT / "modules/v851_protocol.c").read_text(encoding="utf-8")
        cls.protocol_h = (REPO_ROOT / "modules/v851_protocol.h").read_text(encoding="utf-8")
        cls.control_h = (REPO_ROOT / "modules/v851_control_info.h").read_text(encoding="utf-8")
        cls.tlv_app = (REPO_ROOT / "modules/v851_tlv_app.c").read_text(encoding="utf-8")
        cls.wifi = (REPO_ROOT / "modules/v851_wifi.c").read_text(encoding="utf-8")
        cls.wifi_h = (REPO_ROOT / "modules/v851_wifi.h").read_text(encoding="utf-8")

    def test_full_snapshot_waits_sixty_seconds_and_retries_until_queued(self) -> None:
        self.assertIn("V851_TLV_CMD_SNAPSHOT                    0x37U", self.protocol_h)
        self.assertIn("V851_STATE_FULL_INTERVAL_MS               60000UL", self.protocol)
        self.assertIn(
            "V851ProtocolQueueStateMask(V851_TLV_CMD_SNAPSHOT,\n"
            "                                                V851_CONTROL_FULL_MASK)",
            self.protocol,
        )
        self.assertIn("V851_CONTROL_FULL_MASK                   0x03FFU", self.control_h)
        self.assertIn("if(sent_mask == V851_CONTROL_FULL_MASK)", self.protocol)
        self.assertIn("v851_state_full_tick = tick", self.protocol)
        self.assertNotIn("v851_state_snapshot_pending", self.protocol)

    def test_local_updates_report_on_next_500ms_scan_without_rate_limit(self) -> None:
        self.assertIn("V851_STATE_SCAN_INTERVAL_MS               500UL", self.protocol)
        self.assertIn("v851_state_dirty_mask |= V851ControlInfoScanChanged()", self.protocol)
        self.assertIn(
            "V851ProtocolQueueStateMask(V851_TLV_CMD_PROPERTY,\n"
            "                                                v851_state_dirty_mask)",
            self.protocol,
        )
        self.assertIn("v851_state_dirty_mask &= (uint16_t)~sent_mask", self.protocol)
        self.assertNotIn("V851_STATE_REPORT_INTERVAL_MS", self.protocol)

    def test_remote_updates_use_shared_actual_state_scan(self) -> None:
        for removed in (
            "V851_REMOTE_CONFIRM_DEPTH",
            "v851_remote_confirm_masks",
            "v851_remote_confirm_overflow_mask",
            "V851ProtocolAddRemoteConfirmation",
            "remote_confirm_mask",
        ):
            self.assertNotIn(removed, self.protocol)
        self.assertIn("(void)V851ControlInfoApplySegment", self.protocol)
        service_start = self.protocol.index("static void V851ProtocolServiceState")
        service_end = self.protocol.index("static void V851ProtocolServiceTx", service_start)
        service = self.protocol[service_start:service_end]
        local = service.index("if(v851_state_dirty_mask != 0U)")
        full = service.index("if((uint32_t)(tick - v851_state_full_tick)")
        self.assertLess(local, full)
        self.assertEqual(service.count("return;"), 1)

    def test_snapshot_sender_accepts_environment_and_actuator_segments(self) -> None:
        self.assertIn("command != V851_TLV_CMD_SNAPSHOT", self.protocol)
        self.assertIn("command == V851_TLV_CMD_SNAPSHOT", self.protocol)
        self.assertIn(
            "segments[index].struct_type != V851_TLV_STRUCT_ENVIRONMENT",
            self.protocol,
        )
        self.assertIn("segments[index].struct_type < V851_TLV_STRUCT_EXHAUST", self.protocol)
        self.assertIn("segments[index].struct_type > V851_TLV_STRUCT_FILTER", self.protocol)

    def test_two_slot_tlv_queue_and_priority_order(self) -> None:
        self.assertIn("#define V851_TLV_TX_DEPTH                         2U", self.protocol)
        wifi = self.protocol.index("if((v851_wifi_tx_count != 0U)")
        self.assertIn("(V851ProtocolFactoryResetBusy() == 0U)", self.protocol[wifi:])
        tlv = self.protocol.index("if(v851_tlv_tx_count != 0U)", wifi)
        self.assertLess(wifi, tlv)
        self.assertNotIn("v851_ota_tx", self.protocol)
        self.assertNotIn("V851ProtocolSendOtaFrame", self.protocol)
        task_start = self.protocol.index("void V851ProtocolTask(void)")
        task = self.protocol[task_start:]
        alarm = task.index("V851ProtocolServiceAlarm(tick)")
        factory = task.index("V851ProtocolServiceFactory()")
        state = task.index("V851ProtocolServiceState(tick)")
        self.assertLess(alarm, factory)
        self.assertLess(factory, state)

    def test_bootstrap_is_37_6c_and_event_alarm_is_38_6d(self) -> None:
        self.assertIn("V851_TLV_CMD_SNAPSHOT                    0x37U", self.protocol_h)
        self.assertIn("V851_TLV_CMD_BOOTSTRAP_RESULT            0x37U", self.protocol_h)
        self.assertIn("V851_TLV_STRUCT_BOOTSTRAP_RESULT         0x6CU", self.protocol_h)
        self.assertIn("V851_TLV_CMD_EVENT_ALARM                 0x38U", self.protocol_h)
        self.assertIn("V851_TLV_STRUCT_EVENT_ALARM              0x6DU", self.protocol_h)
        for tag in ("EA_IS_ALARM", "EA_CODE", "EA_LEVEL", "EA_RECOVERED", "EA_PAYLOAD"):
            self.assertIn(f"V851_TLV_TAG_{tag}", self.protocol_h)
        self.assertNotIn("V851_TLV_CMD_OTA_STATUS", self.protocol_h)
        self.assertNotIn("OtaAcknowledgeComplete", self.protocol)

    def test_bootstrap_cache_writes_qr_to_planned_dgus_region(self) -> None:
        for name in ("device_sn", "ble_id", "api_endpoint", "bind_status", "qr_url"):
            self.assertIn(name, self.protocol_h)
            self.assertIn(f"v851_bootstrap_result.{name}", self.tlv_app)
        self.assertIn("const V851BootstrapResult *V851BootstrapResultGet(void)", self.tlv_app)
        self.assertIn("return NULL;", self.tlv_app)
        self.assertIn("V851BootstrapApplySegment(segment);", self.tlv_app)
        self.assertIn("V851_BOOTSTRAP_QR_VP_ADDR", self.tlv_app)
        self.assertIn("0x5500U", self.tlv_app)
        self.assertIn("V851_BOOTSTRAP_QR_VP_WORDS", self.tlv_app)
        self.assertIn("64U", self.tlv_app)
        self.assertIn("v851_bootstrap_qr_buffer", self.tlv_app)
        self.assertIn("write_dgus_vp(V851_BOOTSTRAP_QR_VP_ADDR", self.tlv_app)

    def test_application_protocol_has_no_ota_queue_or_status(self) -> None:
        for removed in (
            "V851ProtocolSendOtaFrame", "v851_ota_tx", "OtaCompleteFlag",
            "OtaAcknowledgeComplete", "V851_OTA_STAGE_",
        ):
            self.assertNotIn(removed, self.protocol + self.protocol_h)

    def test_wifi_uses_r11_addresses_and_commands(self) -> None:
        for value in ("0x0600UL", "0x04B0UL", "0x04C0UL", "0x05B8UL", "0x05BEUL", "0x06D8UL"):
            self.assertIn(value, self.wifi_h)
        for command in ("0xC0U", "0xC1U", "0xC5U"):
            self.assertIn(command, self.wifi_h)
        self.assertIn("V851_WIFI_PAGE_SIZE                      5U", self.wifi)
        self.assertIn("V851_WIFI_KEY_PREVIOUS", self.wifi)
        self.assertIn("V851_WIFI_KEY_NEXT", self.wifi)

    def test_wifi_connect_length_is_exact(self) -> None:
        self.assertIn("declared_length = (uint16_t)(offset - 4U)", self.wifi)
        self.assertIn("V851ProtocolSendWifiFrame(frame, offset)", self.wifi)
        self.assertNotIn("offset + 4U", self.wifi)

    def test_c5_updates_connected_or_disconnected_vp(self) -> None:
        self.assertIn("status_word = (frame[8] == 2U) ? 2U : 1U", self.wifi)
        self.assertIn("write_dgus_vp(V851_WIFI_STATUS_ADDR", self.wifi)


if __name__ == "__main__":
    unittest.main()
