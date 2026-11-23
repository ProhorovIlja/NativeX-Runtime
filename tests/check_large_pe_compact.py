"""Generate a large but original PE32+ .pdata fixture; no game file needed.

Default diagnostic retains a bounded small-image JSON limit, while --compact
verifies all unwind entries and emits aggregate counts instead of megabytes.
"""
from pathlib import Path
import json
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from check_pe_intake import fixture


def main(executable):
    with tempfile.TemporaryDirectory() as tmp:
        path = Path(tmp) / "original synthetic large pdata.exe"
        fixture(path)
        data = bytearray(path.read_bytes())
        count = 65_537
        table_rva = 0x300000
        table_raw = 0x1000
        table_bytes = count * 12
        opt = 0x98
        coff = 0x84
        sect2 = opt + 0xF0 + 40
        struct.pack_into('<H', data, coff + 2, 2)
        struct.pack_into('<I', data, opt + 56, 0x400000)
        struct.pack_into('<II', data, opt + 112 + 3*8, table_rva, table_bytes)
        data[sect2:sect2+8] = b'.pdata\x00\x00'
        struct.pack_into('<IIII', data, sect2+8, table_bytes, table_rva, table_bytes, table_raw)
        struct.pack_into('<I', data, sect2+36, 0x40000040)
        data.extend(b'\x00'*(table_raw + table_bytes - len(data)))
        for i in range(count):
            begin = 0x2000 + 16*i
            struct.pack_into('<III', data, table_raw + i*12,
                             begin, begin + 8, 0x1350)
        path.write_bytes(data)
        command = [executable, str(path)]
        unbounded = subprocess.run(command, text=True, capture_output=True)
        assert unbounded.returncode == 2 and 'exception directory' in unbounded.stderr
        compact = subprocess.run(command+['--compact'], text=True,
                                 capture_output=True, check=True)
        info = json.loads(compact.stdout)
        assert info['runtime_function_count'] == count, info.get('runtime_function_count')
        assert info['unwind_info_count'] == count
        assert info['runtime_functions'] == [] and info['unwind_infos'] == []
        assert info['unwind_version_counts'] == {'1':count}, info['unwind_version_counts']
        assert info['unwind_flag_counts'] == {'0':count}, info['unwind_flag_counts']
        assert info['compact_pdata_summary'] and len(compact.stdout) < 4096
        assert info['import_symbols'][0]['iat_rva'] == 0x1250
        # The output remains a read-only inventory, never executable proof.
        assert info['executable_tested'] is False
        assert path.read_bytes()[table_raw:table_raw+12] == struct.pack('<III',0x2000,0x2008,0x1350)
        # Overflow/misaligned entries remain invalid even in compact mode.
        data[table_raw+8:table_raw+12] = b'\xff\xff\xff\xff'
        path.write_bytes(data)
        rejected = subprocess.run(command+['--compact'], text=True,
                                  capture_output=True)
        assert rejected.returncode == 2 and 'pe_intake:' in rejected.stderr
    print('synthetic large PE32+ >65k pdata compact: PASS (no proprietary data)')


if __name__ == '__main__':
    main(sys.argv[1])
