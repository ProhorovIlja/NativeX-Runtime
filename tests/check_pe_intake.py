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
    sh=opt+0xF0
    data[sh:sh+8]=b'.rdata\x00\x00'
    struct.pack_into('<IIII',data,sh+8,0x100,0x1000,0x200,0x200)
    struct.pack_into('<IIIII',data,0x200,0x1050,0,0,0x1040,0x1050)
    data[0x240:0x240+13]=b'KERNEL32.dll\x00'
    path.write_bytes(data)

def main(exe):
    with tempfile.TemporaryDirectory() as temp:
        d=Path(temp); p=d/'fixture with spaces.exe';fixture(p)
        run=subprocess.run([exe,str(p)],capture_output=True,text=True,check=True)
        info=json.loads(run.stdout)
        assert info['machine']=='AMD64' and info['image_base']==0x140000000,info
        assert info['section_names']==['.rdata'] and info['entry_rva']==0x1000,info
        assert info['import_modules']==['KERNEL32.dll'],info
        assert info['executable_tested'] is False,info
        for data in (p.read_bytes()[:160],b'not a PE file'):
            bad=d/'bad.exe';bad.write_bytes(data)
            run=subprocess.run([exe,str(bad)],capture_output=True,text=True)
            assert run.returncode!=0 and 'pe_intake:' in run.stderr
    print("synthetic PE32+ import and corruption test: PASS")
if __name__=='__main__':main(sys.argv[1])
