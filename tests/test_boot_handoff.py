from __future__ import annotations

import unittest
import os
import shutil
import subprocess
import tempfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]


def build_file_info(
    *,
    command: int = 0x04,
    total: int = 1,
    index: int = 0,
    all_size: int = 4096,
    file_size: int = 4096,
    name: bytes = b"22.bin",
    status: int = 0,
) -> bytes:
    body = bytearray([command, total, index])
    body += all_size.to_bytes(4, "little")
    body += bytes([2, 1])
    body += (22).to_bytes(2, "little")
    body += file_size.to_bytes(4, "little")
    body += bytes([len(name)]) + name + bytes([status])
    body += (0x12345678).to_bytes(4, "little")
    return b"\xAB\xCD" + len(body).to_bytes(2, "big") + body


def is_upgrade_frame(frame: bytes) -> bool:
    if not 25 <= len(frame) <= 280 or frame[:2] != b"\xAB\xCD":
        return False
    if int.from_bytes(frame[2:4], "big") != len(frame) - 4:
        return False
    if frame[4] != 0x04 or not 1 <= frame[5] <= 20 or frame[6] != 0:
        return False
    status_index = 20 + frame[19]
    crc_index = status_index + 1
    return (
        crc_index + 4 == len(frame)
        and frame[status_index] == 0
        and int.from_bytes(frame[7:11], "little") != 0
        and int.from_bytes(frame[15:19], "little") != 0
    )


class BootHandoffTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = (REPO_ROOT / "modules/boot_handoff.c").read_text(encoding="utf-8")
        cls.header = (REPO_ROOT / "modules/boot_handoff.h").read_text(encoding="utf-8")
        cls.uart = (REPO_ROOT / "source/uart.c").read_text(encoding="utf-8")

    def test_complete_valid_first_04_is_accepted(self) -> None:
        frame = build_file_info()
        self.assertTrue(is_upgrade_frame(frame))
        self.assertEqual(int.from_bytes(frame[2:4], "big"), len(frame) - 4)
        self.assertEqual(int.from_bytes(frame[13:15], "little"), 22)

    def test_malformed_or_non_first_frames_are_rejected(self) -> None:
        cases = [
            build_file_info(command=0x05),
            build_file_info(total=0),
            build_file_info(total=21),
            build_file_info(index=1),
            build_file_info(all_size=0),
            build_file_info(file_size=0),
            build_file_info(status=1),
            build_file_info()[:-1],
            build_file_info() + b"\x00",
        ]
        bad_length = bytearray(build_file_info())
        bad_length[3] ^= 1
        cases.append(bytes(bad_length))
        for frame in cases:
            with self.subTest(frame=frame.hex()):
                self.assertFalse(is_upgrade_frame(frame))

    def test_handoff_persists_control_before_soft_reset(self) -> None:
        persist = self.source.index("DgusToFlash(flashMAIN_BLOCK_ORDER")
        reset = self.source.index("write_dgus_vp(BOOT_HANDOFF_RESET_ADDR")
        self.assertLess(persist, reset)
        self.assertIn("{0x5AU, 0xA5U, 0x5AU, 0xA5U}", self.source)
        self.assertIn("{0x55U, 0xAAU, 0x5AU, 0xA5U}", self.source)
        self.assertIn("BOOT_HANDOFF_CONTROL_ADDR         0x0020U", self.source)
        self.assertIn("BOOT_HANDOFF_RESET_ADDR           0x0004U", self.source)

    def test_handoff_never_replies_with_05(self) -> None:
        self.assertNotIn("UartSendData", self.source)
        self.assertNotIn("0x05U", self.source)
        self.assertNotIn("V851ProtocolSendOtaFrame", self.source)

    def test_uart_only_hands_abcd_to_handoff_on_uart4(self) -> None:
        self.assertIn("else if((uart == &Uart4)", self.uart)
        self.assertIn("BOOT_HANDOFF_FILE_INFO_MAX - 4U", self.uart)
        self.assertIn("BootHandoffIsUpgradeFrame", self.uart)
        self.assertNotIn("OtaReceive", self.uart)
        self.assertIn("#define BOOT_HANDOFF_FILE_INFO_MAX       280U", self.header)

    def test_control_poll_is_initialized_and_scheduled(self) -> None:
        main = (REPO_ROOT / "user/main.c").read_text(encoding="utf-8")
        self.assertLess(main.index("T5LCpuInit();"), main.index("BootHandoffInit();"))
        self.assertLess(main.index("BootHandoffInit();"), main.index("SysTaskRun();"))
        self.assertIn("SysTaskAdd(6, BOOT_HANDOFF_TASK_INTERVAL, BootHandoffTask);", main)

    def test_production_control_persistence_and_reset(self) -> None:
        compiler = os.environ.get("HOST_CC") or shutil.which("clang") or shutil.which("gcc")
        local = REPO_ROOT.parent / "tools/llvm-mingw-20250709-ucrt-x86_64/bin/clang.exe"
        if not compiler and local.exists():
            compiler = str(local)
        if not compiler:
            self.skipTest("Set HOST_CC to a native clang/gcc compiler")
        with tempfile.TemporaryDirectory(prefix="t5l-handoff-") as directory:
            folder = Path(directory)
            (folder / "sys.h").write_text('''#pragma once
#include <stdint.h>
#include <stddef.h>
#define flashMAIN_BLOCK_ORDER 0U
extern uint8_t EA;
#define SysEnterCritical() EA=0
#define SysExitCritical() EA=1
void write_dgus_vp(uint32_t, uint8_t *, uint16_t);
void read_dgus_vp(uint32_t, uint8_t *, uint8_t);
void DgusToFlash(uint8_t, uint16_t, uint16_t, uint16_t);
''', encoding="utf-8")
            (folder / "test.c").write_text('''#include "boot_handoff.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
uint8_t EA=1;
static uint8_t vp[4], flash[4], expected[4];
static unsigned flash_writes, resets, flash_done;
void write_dgus_vp(uint32_t addr, uint8_t *buf, uint16_t words) {
    const uint8_t reset_word[]={0x55,0xaa,0x5a,0xa5};
    assert(words==2);
    if(addr==0x20) memcpy(vp,buf,4);
    else {
        assert(addr==4 && !EA && flash_done);
        assert(!memcmp(buf,reset_word,4));
        assert(!memcmp(flash,expected,4));
        ++resets;
    }
}
void read_dgus_vp(uint32_t addr, uint8_t *buf, uint8_t words) {
    assert(addr==0x20 && words==2);memcpy(buf,vp,4);
}
void DgusToFlash(uint8_t block, uint16_t addr, uint16_t source, uint16_t words) {
    assert(block==0 && addr==0x20 && source==0x20 && words==2);
    assert(!resets && !memcmp(vp,expected,4));
    memcpy(flash,vp,4);++flash_writes;flash_done=1;
}
static void prepare(const uint8_t *command) {
    memcpy(vp,command,4);memcpy(flash,command,4);memcpy(expected,command,4);
    flash_writes=resets=flash_done=0;
    BootHandoffInit();
    assert(!vp[0] && !vp[1] && !vp[2] && !vp[3]);
    assert(!memcmp(flash,command,4));
    BootHandoffTask();assert(!flash_writes && !resets);
}
static void check_request(const uint8_t *command) {
    prepare(command);
    /* A new request identical to the consumed startup value must still work. */
    memcpy(vp,command,4);BootHandoffTask();
    assert(flash_writes==1 && resets==1 && EA);
    BootHandoffTask();BootHandoffTask();BootHandoffRequestUpgrade();
    assert(flash_writes==1 && resets==1 && !memcmp(flash,command,4));
}
int main(void) {
    const uint8_t upgrade[]={0x5a,0xa5,0x5a,0xa5};
    const uint8_t load[]={0xaa,0x55,0x12,0x34};
    const uint8_t load_zero[]={0xaa,0x55,0,0};
    const uint8_t load_max[]={0xaa,0x55,0xff,0xff};
    const uint8_t invalid[][4]={{0,0,0,0},{0x5a,0xa5,0,0},
        {0x5a,0xa5,0x5a,0},{0xaa,0,0,0x70},{0xff,0xff,0xff,0xff}};
    unsigned i;
    prepare(load);
    for(i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        memcpy(vp,invalid[i],4);BootHandoffTask();
        assert(!flash_writes && !resets && !memcmp(flash,load,4));
    }
    check_request(upgrade);check_request(load);
    check_request(load_zero);check_request(load_max);
    prepare(upgrade);BootHandoffRequestUpgrade();
    assert(flash_writes==1 && resets==1 && !memcmp(flash,upgrade,4));
    puts("PASS startup control consumption, invalid values, upgrade/load persistence before reset, one-shot reset");
    return 0;
}
''', encoding="utf-8")
            executable = folder / ("test.exe" if os.name == "nt" else "test")
            command = [compiler, "-std=c99", "-Wall", "-Wextra", "-Werror",
                       "-I", str(folder), "-I", str(REPO_ROOT / "modules"),
                       str(REPO_ROOT / "modules/boot_handoff.c"), str(folder / "test.c"),
                       "-o", str(executable)]
            result = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
