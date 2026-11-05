"""Execute the production protocol/Wi-Fi/application C code with mocked hardware."""
from pathlib import Path
import os
import re
import shutil
import subprocess
import tempfile
import unittest

from tools import t5l_serial_simulator as sim

ROOT = Path(__file__).resolve().parents[1]


class ResetFirmwareTests(unittest.TestCase):
    def test_reset_wire_format(self):
        frame = bytes.fromhex('AA 55 00 0A 39 6E 00 04 01 00 01 01 3C DF')
        self.assertEqual(sim.encode_tlv_frame(0x39, [(0x6E, [sim.TlvField(1, b'\x01')])]), frame)
        self.assertEqual(sim.decode_tlv_frame(frame).command, 0x39)
        self.assertEqual(sim.crc16_modbus(frame[4:]), 0)
        for index in range(4, len(frame)):
            damaged = bytearray(frame)
            damaged[index] ^= 1
            with self.subTest(index=index), self.assertRaises(sim.ProtocolError):
                sim.decode_tlv_frame(bytes(damaged))
        with self.assertRaises(sim.ProtocolError):
            sim.decode_tlv_frame(bytes.fromhex('AA 55 00 08 39 6E 00 04 01 00 01 01'))
        boundary = sim.encode_tlv_frame(0x39, [(0x6E, [sim.TlvField(1, b'x' * 2035)])])
        self.assertEqual(len(boundary), 2048)
        self.assertEqual(len(sim.decode_tlv_frame(boundary).segments[0].fields[0].value), 2035)
        with self.assertRaises(ValueError):
            sim.encode_tlv_frame(0x39, [(0x6E, [sim.TlvField(1, b'x' * 2036)])])

    def test_reset_requires_existing_password_check(self):
        source = (ROOT / 'modules/t5l_stc.c').read_text(encoding='utf-8')
        branch = source.split('case 0x2801:', 1)[1].split('case 0x2802:', 1)[0]
        success, failure = branch.split('else', 1)
        self.assertLess(success.index('if (compare_passwd(u8buf))'), success.index('V851ProtocolRequestFactoryReset();'))
        self.assertIn('t5l_factory_reset_pending = 1U;', success)
        self.assertNotIn('nor_flash_write', branch)
        self.assertNotIn('Flash_DataInit', branch)
        self.assertNotIn('V851ProtocolRequestFactoryReset', failure)
        self.assertIn('key_value = 0;\n        write_dgus_vp(0x5300', source)
        poll = source.split('void T5L_Stc_Poll(void)', 1)[1].split('//系统配置', 1)[0]
        self.assertLess(poll.index('T5lStcServiceFactoryReset()'), poll.index('Flash_SaveData()'))

    def test_production_c_behavior(self):
        self.run_c_harness()

    def test_network_status(self):
        self.run_c_harness('network')

    def run_c_harness(self, *args):
        compiler = os.environ.get('HOST_CC') or shutil.which('clang') or shutil.which('gcc')
        local = Path('D:/youngwork/tools/llvm-mingw-20250709-ucrt-x86_64/bin/clang.exe')
        if not compiler and local.exists():
            compiler = str(local)
        if not compiler:
            self.skipTest('Set HOST_CC to a native clang/gcc compiler to execute C tests')
        with tempfile.TemporaryDirectory(prefix='t5l-reset-test-') as directory:
            folder = Path(directory)
            (folder / 'sys.h').write_text('''#pragma once
#include <stdint.h>
#define xdata
#define v851PROTOCOL_ENABLED 1
#define v851STATE_FULL_REPORT_ENABLED 0
#define v851FACTORY_SALES_COUNTRY "CN"
#define v851FACTORY_PRODUCT_KEY "TEST"
#define v851FACTORY_MODEL "TEST"
#define v851FACTORY_HARDWARE_VERSION "1"
#define v851FACTORY_FIRMWARE_VERSION "1"
void write_dgus_vp(uint32_t, const uint8_t *, uint16_t);
void read_dgus_vp(uint32_t, uint8_t *, uint16_t);
void SwitchPageById(uint8_t);
uint16_t crc_16(uint8_t *, uint16_t);
''', encoding='utf-8')
            (folder / 'timer.h').write_text('#include "sys.h"\nuint32_t GetSysTick(void);\n')
            (folder / 'uart.h').write_text('''#include "sys.h"
typedef struct { uint16_t TxHead, TxTail; uint8_t TxBusy; } UART_TYPE;
extern UART_TYPE Uart4;
void UartSendData(UART_TYPE *, const uint8_t *, uint16_t);
''')
            # Compile the actual platform CRC and the actual reset branch/service,
            # with flash/password/UI hardware calls supplied by the harness.
            core = (ROOT / 'include/sys.c').read_text(encoding='utf-8')
            crc = core[core.index('uint16_t crc_16 ('):]
            (folder / 'crc_impl.h').write_text(crc[:crc.index('\n}') + 2], encoding='utf-8')
            stc = (ROOT / 'modules/t5l_stc.c').read_text(encoding='utf-8')
            header = (ROOT / 'modules/t5l_stc.h').read_text(encoding='utf-8')
            constants = '\n'.join(re.findall(r'^#define (?:DATALEN|MAIN_ADDR|BACK_ADDR|MAIN_DATA_ADDR|BACK_DATA_ADDR) .*$', header, re.M))
            service = stc.split('static uint8_t T5lStcServiceFactoryReset(void)', 1)[1].split('void T5L_Stc_Poll(void)', 1)[0]
            branch = stc.split('case 0x2801:', 1)[1].split('case 0x2802:', 1)[0]
            (folder / 'factory_reset_stc.h').write_text(constants + '''
static uint8_t t5l_factory_reset_pending;
static uint8_t T5lStcServiceFactoryReset(void)''' + service + '''
static void TestResetKeyScan(void)
{
    uint8_t u8buf[64];
    uint16_t key_value;
    read_dgus_vp(0x5300, (uint8_t *)&key_value, 1);
    switch(key_value) {
    case 0x2801:''' + branch + '''
    default: break;
    }
    key_value = 0;
    write_dgus_vp(0x5300, (uint8_t *)&key_value, 1);
}
''', encoding='utf-8')
            executable = folder / ('test.exe' if os.name == 'nt' else 'test')
            command = [compiler, '-std=c99', '-Wno-unknown-pragmas', '-I', str(folder),
                       '-I', str(ROOT / 'modules'), str(ROOT / 'tests/v851_reset_firmware_harness.c'),
                       '-o', str(executable)]
            built = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            result = subprocess.run([str(executable), *args], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
