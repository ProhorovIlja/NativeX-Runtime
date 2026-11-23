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
    # Two sorted synthetic AMD64 .pdata RUNTIME_FUNCTION entries, metadata only.
    struct.pack_into('<II',data,opt+112+3*8,0x1320,24)
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
    # Self-contained synthetic PE32+ TLS directory at raw offset 0x290.
    # IMAGE_TLS_DIRECTORY64 uses absolute image VA fields, not ordinary RVA.
    base=0x140000000
    struct.pack_into('<QQQQII',data,0x290,
                     base+0x1300,base+0x1308,base+0x1310,
                     base+0x12E0,16,0)
    struct.pack_into('<QQQ',data,0x4E0,base+0x1000,base+0x1010,0)
    data[0x500:0x508]=bytes([7,11,13,17,19,23,29,31])
    struct.pack_into('<I',data,0x510,0)
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
    # The .pdata body is at raw 0x520 (RVA 0x1320). The referenced unwind
    # offsets are decoded as V1 basic operations by our read-only parser.
    struct.pack_into('<IIIIII',data,0x520,
                     0x1000,0x1020,0x1350,
                     0x1020,0x1040,0x1360)
    # Original AMD64 xdata V1: prolog 4, 40-byte UWOP_ALLOC_SMALL;
    # second entry uses 16-byte allocation, 8-byte RBX push and handler RVA.
    data[0x550:0x558]=bytes((0x01,4,1,0,4,0x42,0,0))
    data[0x560:0x568]=bytes((0x09,8,2,0,8,0x12,2,0x30))
    struct.pack_into('<I',data,0x568,0x1090)
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
            {'module':'KERNEL32.dll','name':'GetCurrentProcessId','ordinal':None,
             'lookup_rva':0x1250,'iat_rva':0x1250,'name_hint':0x2a},
            {'module':'KERNEL32.dll','name':None,'ordinal':123,
             'lookup_rva':0x1258,'iat_rva':0x1258,'name_hint':None}
        ],info
        assert info['delay_import_symbols']==[
            {'module':'USER32.dll','name':'MessageBoxW','ordinal':None,
             'lookup_rva':0x12A0,'iat_rva':None,'name_hint':3}
        ],info
        assert info['runtime_functions']==[
            {'begin_rva':0x1000,'end_rva':0x1020,'unwind_rva':0x1350},
            {'begin_rva':0x1020,'end_rva':0x1040,'unwind_rva':0x1360}
        ],info
        assert info['unwind_codes_decoded'] is False,info
        assert info['unwind_codes_executed'] is False,info
        assert info['unwind_infos']==[
            {'unwind_rva':0x1350,'version':1,'flags':0,'prolog_bytes':4,
             'code_slots':1,'decoded_v1_basic_operations':True,
             'stack_allocation_bytes':40,'chained':False,'handler_rva':None},
            {'unwind_rva':0x1360,'version':1,'flags':1,'prolog_bytes':8,
             'code_slots':2,'decoded_v1_basic_operations':True,
             'stack_allocation_bytes':24,'chained':False,'handler_rva':0x1090}
        ],info
        assert info['relocation_blocks']==1,info
        assert info['has_tls_directory'] is True,info
        assert info['tls_template_rva']==0x1300,info
        assert info['tls_template_bytes']==8,info
        assert info['tls_zero_fill_bytes']==16,info
        assert info['tls_index_rva']==0x1310,info
        assert info['tls_callback_rvas']==[0x1000,0x1010],info
        assert info['tls_callbacks_executed'] is False,info
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
        for raw_offset, bad_value in ((0x290, 0xffffffff), # start VA out of image
                                      (0x290+4, 0), # missing image-base high bits
                                      (0x298, 0x12f0), # end < start
                                      (0x290+16, 0xffffffff), # bad TLS index
                                      (0x290+24, 0xffffffff), # bad TLS callback table VA
                                      (0x290+32, 128*1024*1024), # oversized zero-fill
                                      (0x4E0, 0xffffffff), # invalid callback target VA
                                      (0x4E0+16, 0xffffffff), # invalid terminator
                                      (0x98+112+3*8+4, 13), # invalid pdata extent
                                      (0x98+112+3*8, 0xfffffffe), # invalid pdata RVA
                                      (0x520, 0x1020), # empty function body
                                      (0x520+4, 0x1000), # begin >= end
                                      (0x520+8, 0x1351), # unaligned unwind RVA
                                      (0x520+8, 0xffffffff), # out-of-image unwind
                                      (0x520+12, 0x1010), # overlapping function
                                      (0x520+16, 0xffffffff), # out-of-image function
                                      (0x280+4, 0xffffffff),
                                      (0x450, 0xfffffffe), # invalid import name RVA
                                      (0x200+16, 0xfffffff8), # overflow in second IAT slot
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
        # Missing FirstThunk is recorded as unknown rather than inventing
        # a writable IAT slot; name lookup remains available read-only.
        no_iat=bytearray(p.read_bytes())
        struct.pack_into('<I',no_iat,0x200+16,0)
        no_iat_path=d/'missing-first-thunk.exe'
        no_iat_path.write_bytes(no_iat)
        report=json.loads(subprocess.run([exe,str(no_iat_path)],
                         capture_output=True,text=True,check=True).stdout)
        assert all(x['iat_rva'] is None for x in report['import_symbols'])
        assert report['import_symbols'][0]['lookup_rva']==0x1250

        # V1 metadata failures are rejected without loading the executable.
        for offset,value in (
            (0x550,0x00), # invalid version zero
            (0x552,0xff), # truncated code slots
            (0x554,0x7f), # code offset beyond prolog
            (0x555,0x47), # reserved opcode
            (0x555,0x11), # ALLOC_LARGE missing 2 extra slots
            (0x560,0x29), # EHANDLER | CHAININFO conflicts
        ):
            corrupted=bytearray(p.read_bytes())
            corrupted[offset]=value
            filename=d/'invalid-xdata.exe'
            filename.write_bytes(corrupted)
            rejected=subprocess.run([exe,str(filename)],capture_output=True,text=True)
            assert rejected.returncode!=0 and 'pe_intake:' in rejected.stderr,(offset,rejected.stdout,rejected.stderr)
        corrupt_handler=bytearray(p.read_bytes())
        struct.pack_into('<I',corrupt_handler,0x568,0)
        invalid_handler=d/'invalid-unwind-handler.exe'
        invalid_handler.write_bytes(corrupt_handler)
        rejected=subprocess.run([exe,str(invalid_handler)],capture_output=True,text=True)
        assert rejected.returncode!=0 and 'pe_intake:' in rejected.stderr
        # V3 needs a separate decoder. Never interpret newer payload as V1.
        newer=bytearray(p.read_bytes())
        newer[0x550]=0x03
        newer_path=d/'future-unwind-v3.exe'
        newer_path.write_bytes(newer)
        unknown=json.loads(subprocess.run([exe,str(newer_path)],
                                        capture_output=True,text=True,check=True).stdout)
        assert unknown['unwind_infos'][0]['version']==3
        assert unknown['unwind_infos'][0]['decoded_v1_basic_operations'] is False
        assert unknown['unwind_infos'][0]['stack_allocation_bytes'] is None
        # A well-formed 3-slot ALLOC_LARGE form1 is 524288 bytes and
        # must round-trip. Invalid 1-byte and zero-byte forms must fail.
        large=bytearray(p.read_bytes())
        large[0x550:0x55c]=bytes((1,4,3,0,4,0x11,0,0,8,0,0,0))
        large_path=d/'valid-large-allocation.exe'
        large_path.write_bytes(large)
        parsed=json.loads(subprocess.run([exe,str(large_path)],
                           capture_output=True,text=True,check=True).stdout)
        assert parsed['unwind_infos'][0]['stack_allocation_bytes']==524288,parsed
        for bad_size in (0,1,524289):
            bad=bytearray(large)
            struct.pack_into('<I',bad,0x556,bad_size)
            bad_path=d/'bad-large-alloc.exe'
            bad_path.write_bytes(bad)
            result=subprocess.run([exe,str(bad_path)],capture_output=True,text=True)
            assert result.returncode!=0 and 'pe_intake:' in result.stderr,(bad_size,result.stderr)

        # The compact two-slot variant stores the size divided by 8.
        scaled=bytearray(p.read_bytes())
        scaled[0x550:0x558]=bytes((1,4,2,0,4,0x01,17,0))
        scaled_path=d/'valid-scaled-allocation.exe'
        scaled_path.write_bytes(scaled)
        parsed=json.loads(subprocess.run([exe,str(scaled_path)],
                           capture_output=True,text=True,check=True).stdout)
        assert parsed['unwind_infos'][0]['stack_allocation_bytes']==136,parsed
        scaled[0x556]=0
        scaled_path.write_bytes(scaled)
        rejected=subprocess.run([exe,str(scaled_path)],capture_output=True,text=True)
        assert rejected.returncode!=0 and 'pe_intake:' in rejected.stderr

        # Common CHAININFO flag remains observable in opaque V2 and V3.
        for version in (2,3):
            chained=bytearray(p.read_bytes())
            chained[0x550]=(4<<3)|version
            chained_path=d/'future-chained-unwind.exe'
            chained_path.write_bytes(chained)
            parsed=json.loads(subprocess.run([exe,str(chained_path)],
                               capture_output=True,text=True,check=True).stdout)
            item=parsed['unwind_infos'][0]
            assert item['version']==version and item['flags']==4,parsed
            assert item['chained'] is True and item['decoded_v1_basic_operations'] is False,parsed
            assert item['stack_allocation_bytes'] is None,parsed
    print("synthetic PE32+ named imports, bounded V1 xdata ops, TLS, exceptions and corruption: PASS")
if __name__=='__main__':main(sys.argv[1])
