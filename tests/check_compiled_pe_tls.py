"""Independent PE32+ metadata acceptance against a real source-built MSVC TLS DLL.

Read-only parsing and corrupted copies only. The separate native Win32 CTest
actually LoadLibrary's the same DLL and verifies its callback/thread behavior.
No commercial binaries or third-party SDKs are used.
"""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def inspect(parser, image, *, should_pass=True):
    result = subprocess.run(
        [str(parser), str(image)], capture_output=True, text=True,
        timeout=12, check=False
    )
    if not should_pass:
        assert result.returncode != 0, (
            "corrupted real MSVC PE accepted", result.stdout, result.stderr
        )
        assert "pe_intake:" in result.stderr, result.stderr
        return None
    assert result.returncode == 0, (result.stdout, result.stderr)
    return json.loads(result.stdout)


def raw_offset(data, address):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[pe:pe + 4] == b"PE\x00\x00"
    count = struct.unpack_from("<H", data, pe + 6)[0]
    opt_length = struct.unpack_from("<H", data, pe + 20)[0]
    sections = pe + 24 + opt_length
    for index in range(count):
        sh = sections + index * 40
        va = struct.unpack_from("<I", data, sh + 12)[0]
        raw_size = struct.unpack_from("<I", data, sh + 16)[0]
        raw_start = struct.unpack_from("<I", data, sh + 20)[0]
        if va <= address < va + raw_size:
            target = raw_start + address - va
            assert 0 <= target < len(data)
            return target
    raise AssertionError("section RVA has no on-disk bytes")


def main(parser, dll):
    obj = inspect(parser, dll)
    assert obj["kind"] == "pe32plus-metadata", obj
    assert obj["machine"] == "AMD64", obj
    assert obj["has_tls_directory"] is True, obj
    assert obj["tls_callbacks_executed"] is False, obj
    assert isinstance(obj["tls_callback_rvas"], list), obj
    assert 1 <= len(obj["tls_callback_rvas"]) <= 256, obj
    assert all(isinstance(rva, int) and rva > 0
               for rva in obj["tls_callback_rvas"]), obj
    assert obj["tls_template_rva"] is not None, obj
    assert obj["tls_template_bytes"] >= 4, obj
    assert obj["tls_index_rva"] is not None, obj
    assert obj["tls_zero_fill_bytes"] >= 0, obj
    assert obj["unwind_codes_decoded"] is False, obj
    assert obj["runtime_functions"], obj
    assert all(fn["begin_rva"] < fn["end_rva"]
               for fn in obj["runtime_functions"]), obj

    data = bytearray(Path(dll).read_bytes())
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    opt = pe + 24
    tls_dir = opt + 112 + 9 * 8
    exception_dir = opt + 112 + 3 * 8
    tls_rva = struct.unpack_from("<I", data, tls_dir)[0]
    assert tls_rva != 0
    callback_va_pos = raw_offset(data, tls_rva) + 24

    with tempfile.TemporaryDirectory() as temporary:
        bad_path = Path(temporary) / "deliberately-corrupted-source-built.dll"
        changed = bytearray(data)
        struct.pack_into("<Q", changed, callback_va_pos,
                         0xFFFFFFFFFFFFFFFF)  # invalid callback table VA
        bad_path.write_bytes(changed)
        inspect(parser, bad_path, should_pass=False)

        changed = bytearray(data)
        size = struct.unpack_from("<I", changed, exception_dir + 4)[0]
        assert size > 0
        struct.pack_into("<I", changed, exception_dir + 4, size + 1)
        bad_path.write_bytes(changed)
        inspect(parser, bad_path, should_pass=False)

    print(json.dumps({
        "kind": "source-built-msvc-tls-and-pdata-inspection",
        "tls_callback_count": len(obj["tls_callback_rvas"]),
        "runtime_function_count": len(obj["runtime_functions"]),
        "tls_template_bytes": obj["tls_template_bytes"],
        "corrupted_callback_va_rejected": True,
        "malformed_pdata_size_rejected": True,
        "pe_image_executed_by_parser": False,
        "commercial_game_tested": False,
    }))


if __name__ == "__main__":
    main(Path(sys.argv[1]), Path(sys.argv[2]))
