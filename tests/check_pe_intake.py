"""Synthetic, non-executable PE32+ fixture. No proprietary content."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

def fixture(path):
    data=bytearray(0x900)
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
    # A separate export table (3 names and ordinals 5, 6, 7).
    struct.pack_into('<II',data,opt+112,0x1140,0xC0)
    # Relocations, TLS and delay imports are all synthesized, never copied.
    struct.pack_into('<II',data,opt+112+5*8,0x1080,8)
    struct.pack_into('<II',data,opt+112+9*8,0x1090,40)
    struct.pack_into('<II',data,opt+112+13*8,0x10C0,64)
    sh=opt+0xF0
    data[sh:sh+8]=b'.rdata\x00\x00'
    struct.pack_into('<IIII',data,sh+8,0x400,0x1000,0x400,0x200)
    struct.pack_into('<IIIII',data,0x200,0x1250,0,0,0x1040,0x1250)
    data[0x240:0x240+13]=b'KERNEL32.dll\x00'
    struct.pack_into('<II',data,0x280,0x1000,8)
    # Zero-initialized synthetic PE32+ TLS directory at raw offset 0x290.
    struct.pack_into('<II',data,0x2c0,1,0x1100)
    struct.pack_into('<I',data,0x2c0+16,0x12A0)  # delay import INT
    data[0x300:0x300+11]=b'USER32.dll\x00'
    # IMAGE_EXPORT_DIRECTORY: three named exports, one forwarded by RVA.
    struct.pack_into('<IIHHIIIIIII',data,0x340,
                     0,0,0,0,0x11A0,5,3,3,0x1180,0x118C,0x1198)
    struct.pack_into('<III',data,0x380,0x1300,0x1310,0x11EE)
    struct.pack_into('<III',data,0x38C,0x11B4,0x11C4,0x11DC)
    struct.pack_into('<HHH',data,0x398,0,1,2)
    data[0x3A0:0x3A0+len(b'synthetic.dll\x00')]=b'synthetic.dll\x00'
    data[0x3B4:0x3B4+len(b'ScarlettAdd\x00')]=b'ScarlettAdd\x00'
    data[0x3C4:0x3C4+len(b'ScarlettMultiply\x00')]=b'ScarlettMultiply\x00'
    data[0x3DC:0x3DC+len(b'AliasForward\x00')]=b'AliasForward\x00'
    data[0x3EE:0x3EE+len(b'KERNEL32.Sleep\x00')]=b'KERNEL32.Sleep\x00'
    # Original IMAGE_THUNK_DATA64: named hint/name, ordinal, terminator.
    struct.pack_into('<QQQ',data,0x450,0x1270,0x800000000000007B,0)
    struct.pack_into('<H',data,0x470,0x002A)
    data[0x472:0x472+len(b'GetCurrentProcessId\x00')]=b'GetCurrentProcessId\x00'
    # Delay imported USER32.MessageBoxW through a separate INT table.
    struct.pack_into('<QQ',data,0x4A0,0x12C0,0)
    struct.pack_into('<H',data,0x4C0,3)
    data[0x4C2:0x4C2+len(b'MessageBoxW\x00')]=b'MessageBoxW\x00'
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
        assert info['import_symbols']==[
            {'module':'KERNEL32.dll','name':'GetCurrentProcessId','ordinal':None},
            {'module':'KERNEL32.dll','name':None,'ordinal':123}
        ],info
        assert info['delay_import_symbols']==[
            {'module':'USER32.dll','name':'MessageBoxW','ordinal':None}
        ],info
        assert info['relocation_blocks']==1,info
        assert info['has_tls_directory'] is True,info
        assert info['exports']==[
            {'name':'ScarlettAdd','ordinal':5,'forwarder':None},
            {'name':'ScarlettMultiply','ordinal':6,'forwarder':None},
            {'name':'AliasForward','ordinal':7,'forwarder':'KERNEL32.Sleep'}
        ],info
        assert info['executable_tested'] is False,info
        for data in (p.read_bytes()[:160],b'not a PE file'):
            bad=d/'bad.exe';bad.write_bytes(data)
            run=subprocess.run([exe,str(bad)],capture_output=True,text=True)
            assert run.returncode!=0 and 'pe_intake:' in run.stderr
        # Negative cases: malformed exports and imports must fail without code execution.
        for raw_offset, bad_value in ((0x280+4, 0xffffffff),
                                      (0x450, 0xfffffffe), # invalid import name RVA
                                      (0x450+4, 0x00000001), # invalid 64-bit name high word
                                      (0x458+4, 0xc0000000), # ordinal has forbidden flag bits
                                      (0x4a0, 0xfffffffe), # bad delay import INT name
                                      (0x2c0+16, 0xfffffffe), # bad delay lookup RVA
                                      (0x98+112+9*8+4, 8),
                                      (0x2c0, 0),
                                      (0x340+20, 0xffffffff),
                                      (0x340+28, 0xffffffff),
                                      (0x398, 0xffff)):
            corrupted=bytearray(p.read_bytes())
            struct.pack_into('<I',corrupted,raw_offset,bad_value)
            bad=d/'bad-metadata.exe'
            bad.write_bytes(corrupted)
            run=subprocess.run([exe,str(bad)],capture_output=True,text=True)
            assert run.returncode!=0 and 'pe_intake:' in run.stderr,(raw_offset,run.stdout,run.stderr)
    print("synthetic PE32+ named/ordinal imports, delayed thunks, exports, TLS, relocations and corruption: PASS")
if __name__=='__main__':main(sys.argv[1])
