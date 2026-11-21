"""Synthetic, non-executable PE32+ fixture. No proprietary content."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

def fixture(path):
    data=bytearray(0x800)
    struct.pack_into('<H',data,0,0x5A4D)
    struct.pack_into('<I',data,0x3C,0x80)
    data[0x80:0x84]=b'PE\x00\x00'
    struct.pack_into('<HHIIIHH',data,0x84,0x8664,1,0,0,0,0xF0,0x0022)
    opt=0x98
    struct.pack_into('<H',data,opt,0x20B)
    struct.pack_into('<I',data,opt+16,0x1000)
    struct.pack_into('<Q',data,opt+24,0x140000000)
    struct.pack_into('<I',data,opt+56,0x2000)
    struct.pack_into('<I',data,opt+60,0x200)
    struct.pack_into('<I',data,opt+108,16)
    struct.pack_into('<II',data,opt+120,0x1000,40)
    # Relocations, TLS and delay imports are all synthesized, never copied.
    struct.pack_into('<II',data,opt+112+5*8,0x1080,8)
    struct.pack_into('<II',data,opt+112+9*8,0x1090,40)
    struct.pack_into('<II',data,opt+112+13*8,0x10C0,64)
    sh=opt+0xF0
    data[sh:sh+8]=b'.rdata\x00\x00'
    struct.pack_into('<IIII',data,sh+8,0x100,0x1000,0x200,0x200)
    struct.pack_into('<IIIII',data,0x200,0x1050,0,0,0x1040,0x1050)
    data[0x240:0x240+13]=b'KERNEL32.dll\x00'
    struct.pack_into('<II',data,0x280,0x1000,8)
    # Zero-initialized synthetic PE32+ TLS directory at raw offset 0x290.
    struct.pack_into('<II',data,0x2c0,1,0x1100)
    data[0x300:0x300+11]=b'USER32.dll\x00'
    path.write_bytes(data)

def main(exe):
    with tempfile.TemporaryDirectory() as temp:
        d=Path(temp); p=d/'fixture with spaces.exe';fixture(p)
        run=subprocess.run([exe,str(p)],capture_output=True,text=True,check=True)
        info=json.loads(run.stdout)
        assert info['machine']=='AMD64' and info['image_base']==0x140000000,info
        assert info['section_names']==['.rdata'] and info['entry_rva']==0x1000,info
        assert info['import_modules']==['KERNEL32.dll'],info
        assert info['delay_import_modules']==['USER32.dll'],info
        assert info['relocation_blocks']==1,info
        assert info['has_tls_directory'] is True,info
        assert info['executable_tested'] is False,info
        for data in (p.read_bytes()[:160],b'not a PE file'):
            bad=d/'bad.exe';bad.write_bytes(data)
            run=subprocess.run([exe,str(bad)],capture_output=True,text=True)
            assert run.returncode!=0 and 'pe_intake:' in run.stderr
        # Negative cases: malformed metadata must fail without executing the image.
        for raw_offset, bad_value in ((0x280+4, 0xffffffff),
                                      (0x98+112+9*8+4, 8),
                                      (0x2c0, 0)):
            corrupted=bytearray(p.read_bytes())
            struct.pack_into('<I',corrupted,raw_offset,bad_value)
            bad=d/'bad-metadata.exe'
            bad.write_bytes(corrupted)
            run=subprocess.run([exe,str(bad)],capture_output=True,text=True)
            assert run.returncode!=0 and 'pe_intake:' in run.stderr,(raw_offset,run.stdout,run.stderr)
    print("synthetic PE32+ imports, delay imports, TLS, relocations and corruption: PASS")
if __name__=='__main__':main(sys.argv[1])
